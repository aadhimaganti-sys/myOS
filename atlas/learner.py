"""
Atlas Machine Learning & Dynamic Adaptive Memory Engine
Continuously trains on user conversations and adapts its responses.
"""

import json
import math
import os
import re
from collections import defaultdict
from pathlib import Path

MEMORY_FILE = Path(__file__).resolve().parent / "memory.json"

class AtlasLearner:
    def __init__(self):
        self.vocab = set()
        self.transitions = defaultdict(lambda: defaultdict(int)) # context -> next_word -> count
        self.concept_graph = defaultdict(lambda: defaultdict(float)) # word -> related_word -> weight
        self.facts = [] # learned explicit facts / rules
        self.history = [] # list of {"user": ..., "atlas": ...}
        self.training_steps = 0
        self.load_memory()

    def tokenize(self, text: str):
        text = text.lower()
        tokens = re.findall(r"\b[a-z0-9_']+\b", text)
        return tokens

    def train_on_input(self, text: str):
        """Online training step: updates transitions, vocabulary, and concept graph."""
        tokens = self.tokenize(text)
        if not tokens:
            return

        self.training_steps += 1
        for token in tokens:
            self.vocab.add(token)

        # 1. Train Bigram & Trigram Markov Language Model
        for i in range(len(tokens) - 1):
            ctx1 = tokens[i]
            next_w = tokens[i + 1]
            self.transitions[ctx1][next_w] += 1

            if i < len(tokens) - 2:
                ctx2 = f"{tokens[i]} {tokens[i+1]}"
                next_w2 = tokens[i + 2]
                self.transitions[ctx2][next_w2] += 1

        # 2. Train Co-occurrence Concept Association Matrix (Hebbian learning: neurons that fire together wire together)
        for i, t1 in enumerate(tokens):
            for j, t2 in enumerate(tokens):
                if i != j:
                    dist = abs(i - j)
                    weight = 1.0 / dist
                    self.concept_graph[t1][t2] += weight

        # 3. Detect and extract explicit semantic facts
        self._extract_semantic_facts(text)

        # 4. Save learned weights to disk
        self.save_memory()

    def _extract_semantic_facts(self, text: str):
        text_clean = text.strip()
        if text_clean.endswith("?") or re.match(r"^(what|who|where|when|why|how|is|are|can|do|does)\b", text_clean, re.I):
            return

        patterns = [
            r"(?:remember that|note that|learn that)\s+(.*)",
            r"(?:my name is|i am)\s+([a-zA-Z0-9_]+)",
            r"(.*)\s+is\s+(.*)",
            r"(?:i like|i love|i prefer)\s+(.*)"
        ]

        text_clean = text.strip()
        for pat in patterns:
            m = re.match(pat, text_clean, re.IGNORECASE)
            if m:
                fact = text_clean
                if fact not in self.facts:
                    self.facts.append(fact)
                break

    def query_learned_knowledge(self, prompt: str):
        """Searches memory and concept graph to generate a personalized response."""
        tokens = self.tokenize(prompt)
        if not tokens:
            return None

        # Check explicit facts first
        relevant_facts = []
        for fact in self.facts:
            fact_tokens = set(self.tokenize(fact))
            overlap = set(tokens).intersection(fact_tokens)
            if overlap:
                relevant_facts.append((len(overlap), fact))

        if relevant_facts:
            relevant_facts.sort(key=lambda x: x[0], reverse=True)
            best_fact = relevant_facts[0][1]
            return f"Based on what you previously taught me: \"{best_fact}\"."

        # Check concept graph associations
        scores = defaultdict(float)
        for t in tokens:
            if t in self.concept_graph:
                for related, weight in self.concept_graph[t].items():
                    if related not in tokens:
                        scores[related] += weight

        if scores:
            top_associated = sorted(scores.items(), key=lambda x: x[1], reverse=True)[:4]
            associations = [w for w, _ in top_associated]
            return f"From our past training, I associate that with: {', '.join(associations)}."

        return None

    def generate_trained_phrase(self, seed: str = None, max_length: int = 12):
        """Generates text from the learned transition model."""
        import random
        if not self.transitions:
            return "My neural weights are still initializing. Teach me more sentences!"

        if not seed or seed not in self.transitions:
            seed = random.choice(list(self.transitions.keys()))

        words = seed.split()
        current = seed

        for _ in range(max_length):
            if current in self.transitions:
                candidates = self.transitions[current]
                total = sum(candidates.values())
                r = random.uniform(0, total)
                upto = 0
                chosen = None
                for word, count in candidates.items():
                    if upto + count >= r:
                        chosen = word
                        break
                    upto += count
                if not chosen:
                    chosen = random.choice(list(candidates.keys()))

                words.append(chosen)
                current = chosen
            else:
                break

        return " ".join(words)

    def get_stats(self):
        num_transitions = sum(len(v) for v in self.transitions.values())
        return {
            "training_steps": self.training_steps,
            "vocabulary_size": len(self.vocab),
            "learned_transitions": num_transitions,
            "facts_stored": len(self.facts),
            "total_conversations": len(self.history)
        }

    def save_memory(self):
        data = {
            "training_steps": self.training_steps,
            "vocab": list(self.vocab),
            "transitions": {k: dict(v) for k, v in self.transitions.items()},
            "concept_graph": {k: dict(v) for k, v in self.concept_graph.items()},
            "facts": self.facts,
            "history": self.history[-100:]
        }
        with open(MEMORY_FILE, "w", encoding="utf-8") as f:
            json.dump(data, f, indent=2)

    def load_memory(self):
        if not MEMORY_FILE.exists():
            return

        try:
            with open(MEMORY_FILE, "r", encoding="utf-8") as f:
                data = json.load(f)
            self.training_steps = data.get("training_steps", 0)
            self.vocab = set(data.get("vocab", []))
            self.facts = data.get("facts", [])
            self.history = data.get("history", [])

            for k, v in data.get("transitions", {}).items():
                self.transitions[k] = defaultdict(int, v)

            for k, v in data.get("concept_graph", {}).items():
                self.concept_graph[k] = defaultdict(float, v)
        except Exception:
            pass

    def clear_memory(self):
        self.vocab.clear()
        self.transitions.clear()
        self.concept_graph.clear()
        self.facts.clear()
        self.history.clear()
        self.training_steps = 0
        if MEMORY_FILE.exists():
            os.remove(MEMORY_FILE)
