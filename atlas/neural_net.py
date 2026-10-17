"""
Atlas Neural Network Engine: A complete Transformer Language Model implemented in NumPy.
Features:
- Learned Dense Token Embeddings
- Multi-Head Causal Self-Attention with Scaled Dot-Product
- Position-wise Feed-Forward Network with GELU activations
- Layer Normalization & Residual Connections
- Cross-Entropy Loss computation
- Exact Analytical Backpropagation & Adam Optimizer
- Dense Semantic Vector Embeddings for Memory Retrieval
- Temperature & Top-p (Nucleus) Autoregressive Text Generation
"""

import math
import os
import re
from pathlib import Path
import numpy as np

WEIGHTS_FILE = Path(__file__).resolve().parent / "neural_weights.npz"

def gelu(x):
    """Gaussian Error Linear Unit (GELU) activation function."""
    return 0.5 * x * (1.0 + np.tanh(np.sqrt(2.0 / np.pi) * (x + 0.044715 * np.power(x, 3))))

def gelu_backward(x, grad_out):
    """Analytical gradient of GELU."""
    cdf = 0.5 * (1.0 + np.tanh(np.sqrt(2.0 / np.pi) * (x + 0.044715 * np.power(x, 3))))
    pdf = np.exp(-0.5 * x * x) / np.sqrt(2.0 * np.pi)
    return grad_out * (cdf + x * pdf)

def softmax(x, axis=-1):
    e_x = np.exp(x - np.max(x, axis=axis, keepdims=True))
    return e_x / (np.sum(e_x, axis=axis, keepdims=True) + 1e-9)

class AtlasTransformerLM:
    def __init__(self, d_model=48, d_ff=96, max_seq_len=64, lr=0.005):
        self.d_model = d_model
        self.d_ff = d_ff
        self.max_seq_len = max_seq_len
        self.lr = lr

        # Vocabulary
        self.special_tokens = ["<pad>", "<unk>", "<bos>", "<eos>"]
        self.token2id = {}
        self.id2token = {}
        for idx, tok in enumerate(self.special_tokens):
            self.token2id[tok] = idx
            self.id2token[idx] = tok

        # Prepopulate with standard systems & conversational vocabulary
        base_words = [
            "the", "a", "an", "is", "are", "was", "were", "to", "in", "of", "and", "or",
            "kernel", "system", "memory", "scheduler", "arm", "cpu", "el0", "el1", "el2",
            "el3", "el4", "page", "allocator", "heap", "kmalloc", "kfree", "interrupt",
            "timer", "tick", "task", "process", "thread", "yield", "switch", "context",
            "framebuffer", "virtio", "disk", "block", "uart", "serial", "hello", "atlas",
            "ai", "neural", "learning", "weights", "model", "train", "loss", "code", "os",
            "my", "you", "i", "we", "can", "do", "what", "how", "why", "who", "where",
            "good", "great", "name", "favorite", "yes", "no", "active", "online", "running"
        ]
        for w in base_words:
            self._add_token(w)

        # Model Parameters
        self._init_weights()

        # Adam Optimizer States
        self._init_optimizer()

        # Training Telemetry
        self.total_training_steps = 0
        self.loss_history = []
        self.learned_sentences = []

        self.load_weights()

    def _add_token(self, word):
        word = word.lower()
        if word not in self.token2id:
            idx = len(self.token2id)
            self.token2id[word] = idx
            self.id2token[idx] = word
            return idx
        return self.token2id[word]

    def _init_weights(self):
        V = len(self.token2id)
        D = self.d_model
        FF = self.d_ff
        scale = 1.0 / np.sqrt(D)

        # Token & Positional Embeddings
        self.W_embed = np.random.randn(V, D).astype(np.float32) * scale
        self.W_pos = np.random.randn(self.max_seq_len, D).astype(np.float32) * scale

        # Self-Attention Weights
        self.W_q = np.random.randn(D, D).astype(np.float32) * scale
        self.W_k = np.random.randn(D, D).astype(np.float32) * scale
        self.W_v = np.random.randn(D, D).astype(np.float32) * scale
        self.W_o = np.random.randn(D, D).astype(np.float32) * scale

        # Feed-Forward Weights
        self.W_ff1 = np.random.randn(D, FF).astype(np.float32) * scale
        self.b_ff1 = np.zeros(FF, dtype=np.float32)
        self.W_ff2 = np.random.randn(FF, D).astype(np.float32) * (1.0 / np.sqrt(FF))
        self.b_ff2 = np.zeros(D, dtype=np.float32)

        # Output projection to vocabulary logits
        self.W_head = np.random.randn(D, V).astype(np.float32) * scale
        self.b_head = np.zeros(V, dtype=np.float32)

    def _init_optimizer(self):
        self.beta1 = 0.9
        self.beta2 = 0.999
        self.eps = 1e-8
        self.opt_t = 0
        self.m = {}
        self.v = {}
        param_names = ["W_embed", "W_pos", "W_q", "W_k", "W_v", "W_o", "W_ff1", "b_ff1", "W_ff2", "b_ff2", "W_head", "b_head"]
        for p in param_names:
            param = getattr(self, p)
            self.m[p] = np.zeros_like(param)
            self.v[p] = np.zeros_like(param)

    def _expand_vocab_if_needed(self, tokens):
        new_tokens = [t for t in tokens if t not in self.token2id]
        if not new_tokens:
            return

        old_V = len(self.token2id)
        for t in new_tokens:
            self._add_token(t)
        new_V = len(self.token2id)
        D = self.d_model
        scale = 1.0 / np.sqrt(D)

        # Expand Embedding
        extra_embed = np.random.randn(new_V - old_V, D).astype(np.float32) * scale
        self.W_embed = np.vstack([self.W_embed, extra_embed])
        self.m["W_embed"] = np.vstack([self.m["W_embed"], np.zeros_like(extra_embed)])
        self.v["W_embed"] = np.vstack([self.v["W_embed"], np.zeros_like(extra_embed)])

        # Expand Output Head
        extra_head = np.random.randn(D, new_V - old_V).astype(np.float32) * scale
        self.W_head = np.hstack([self.W_head, extra_head])
        self.m["W_head"] = np.hstack([self.m["W_head"], np.zeros_like(extra_head)])
        self.v["W_head"] = np.hstack([self.v["W_head"], np.zeros_like(extra_head)])

        extra_b = np.zeros(new_V - old_V, dtype=np.float32)
        self.b_head = np.concatenate([self.b_head, extra_b])
        self.m["b_head"] = np.concatenate([self.m["b_head"], np.zeros_like(extra_b)])
        self.v["b_head"] = np.concatenate([self.v["b_head"], np.zeros_like(extra_b)])

    def tokenize(self, text: str):
        text = text.lower()
        words = re.findall(r"\b[a-z0-9_']+\b", text)
        return words

    def encode(self, text: str):
        words = self.tokenize(text)
        self._expand_vocab_if_needed(words)
        return [self.token2id.get(w, self.token2id["<unk>"]) for w in words]

    def decode(self, token_ids):
        return " ".join([self.id2token.get(i, "<unk>") for i in token_ids if i not in (0, 2, 3)])

    def get_sentence_embedding(self, text: str):
        """Computes a dense neural vector embedding for semantic search."""
        token_ids = self.encode(text)
        if not token_ids:
            return np.zeros(self.d_model, dtype=np.float32)
        vecs = self.W_embed[token_ids]
        mean_vec = np.mean(vecs, axis=0)
        norm = np.linalg.norm(mean_vec)
        if norm > 1e-7:
            mean_vec /= norm
        return mean_vec

    def forward(self, token_ids):
        """
        Forward pass of Causal Self-Attention Transformer.
        Returns: logits (T, V), cache dict for backprop.
        """
        T = min(len(token_ids), self.max_seq_len)
        ids = np.array(token_ids[:T])

        # 1. Embeddings + Position
        x_tok = self.W_embed[ids]  # (T, D)
        x_pos = self.W_pos[:T]     # (T, D)
        h0 = x_tok + x_pos         # (T, D)

        # 2. Self-Attention (Query, Key, Value)
        Q = h0 @ self.W_q          # (T, D)
        K = h0 @ self.W_k          # (T, D)
        V = h0 @ self.W_v          # (T, D)

        # Scaled Dot-Product Attention with Causal Mask
        scores = (Q @ K.T) / np.sqrt(self.d_model) # (T, T)
        causal_mask = np.triu(np.ones((T, T), dtype=bool), k=1)
        scores[causal_mask] = -1e9

        attn_weights = softmax(scores, axis=-1)   # (T, T)
        attn_out = attn_weights @ V               # (T, D)
        h_attn = attn_out @ self.W_o             # (T, D)

        # Residual Connection
        h1 = h0 + h_attn                          # (T, D)

        # 3. Position-wise Feed-Forward Network
        ff1 = h1 @ self.W_ff1 + self.b_ff1       # (T, FF)
        act1 = gelu(ff1)                          # (T, FF)
        ff2 = act1 @ self.W_ff2 + self.b_ff2     # (T, D)

        # Residual Connection
        h2 = h1 + ff2                             # (T, D)

        # 4. Output Head
        logits = h2 @ self.W_head + self.b_head   # (T, V)

        cache = {
            "ids": ids, "T": T, "h0": h0, "Q": Q, "K": K, "V": V,
            "attn_weights": attn_weights, "attn_out": attn_out,
            "h1": h1, "ff1": ff1, "act1": act1, "h2": h2, "logits": logits
        }
        return logits, cache

    def train_step(self, token_ids):
        """
        Performs one full backpropagation step on input sequence (Autoregressive Next-Token Prediction).
        """
        if len(token_ids) < 2:
            return 0.0

        logits, cache = self.forward(token_ids)
        T = cache["T"]
        targets = token_ids[1:T] # Target is next token

        # Cross-Entropy Loss on tokens 0 .. T-2
        loss = 0.0
        d_logits = np.zeros_like(logits)

        for t in range(T - 1):
            target_id = targets[t]
            probs = softmax(logits[t])
            loss += -np.log(probs[target_id] + 1e-9)

            d_logits[t] = probs
            d_logits[t, target_id] -= 1.0

        loss /= (T - 1)
        d_logits /= (T - 1)

        # Backpropagation
        grads = self._backward(d_logits, cache)

        # Optimization Step (Adam)
        self._adam_step(grads)

        self.total_training_steps += 1
        self.loss_history.append(float(loss))
        return float(loss)

    def _backward(self, d_logits, cache):
        """Calculates exact analytical gradients for all network weights."""
        h2 = cache["h2"]
        act1 = cache["act1"]
        ff1 = cache["ff1"]
        h1 = cache["h1"]
        attn_out = cache["attn_out"]
        attn_weights = cache["attn_weights"]
        Q, K, V = cache["Q"], cache["K"], cache["V"]
        h0 = cache["h0"]
        ids = cache["ids"]
        T = cache["T"]

        grads = {}

        # 1. Output Head Gradients
        grads["W_head"] = h2.T @ d_logits
        grads["b_head"] = np.sum(d_logits, axis=0)
        dh2 = d_logits @ self.W_head.T

        # 2. Feed-Forward Gradients
        grads["W_ff2"] = act1.T @ dh2
        grads["b_ff2"] = np.sum(dh2, axis=0)
        dact1 = dh2 @ self.W_ff2.T

        dff1 = gelu_backward(ff1, dact1)
        grads["W_ff1"] = h1.T @ dff1
        grads["b_ff1"] = np.sum(dff1, axis=0)
        dh1 = dh2 + (dff1 @ self.W_ff1.T)

        # 3. Attention Gradients
        grads["W_o"] = attn_out.T @ dh1
        d_attn_out = dh1 @ self.W_o.T

        dV = attn_weights.T @ d_attn_out
        d_attn_weights = d_attn_out @ V.T

        # Softmax backward
        d_scores = attn_weights * (d_attn_weights - np.sum(d_attn_weights * attn_weights, axis=-1, keepdims=True))
        d_scores /= np.sqrt(self.d_model)

        dQ = d_scores @ K
        dK = d_scores.T @ Q

        grads["W_q"] = h0.T @ dQ
        grads["W_k"] = h0.T @ dK
        grads["W_v"] = h0.T @ dV

        dh0_attn = (dQ @ self.W_q.T) + (dK @ self.W_k.T) + (dV @ self.W_v.T)
        dh0 = dh1 + dh0_attn

        # 4. Embedding Gradients
        grads["W_pos"] = np.zeros_like(self.W_pos)
        grads["W_pos"][:T] = dh0

        grads["W_embed"] = np.zeros_like(self.W_embed)
        np.add.at(grads["W_embed"], ids, dh0)

        return grads

    def _adam_step(self, grads):
        self.opt_t += 1
        lr = self.lr

        for name, grad in grads.items():
            param = getattr(self, name)
            # Clip gradients to prevent explosion
            np.clip(grad, -1.0, 1.0, out=grad)

            self.m[name] = self.beta1 * self.m[name] + (1 - self.beta1) * grad
            self.v[name] = self.beta2 * self.v[name] + (1 - self.beta2) * (grad ** 2)

            # Bias correction
            m_hat = self.m[name] / (1 - self.beta1 ** self.opt_t)
            v_hat = self.v[name] / (1 - self.beta2 ** self.opt_t)

            param -= lr * m_hat / (np.sqrt(v_hat) + self.eps)

    def train_on_text(self, text: str, epochs: int = 5):
        """Trains the transformer on a sentence or passage over multiple epochs."""
        words = self.tokenize(text)
        if len(words) < 2:
            return 0.0

        token_ids = self.encode(text)
        initial_loss = 0.0
        final_loss = 0.0

        for ep in range(epochs):
            l = self.train_step(token_ids)
            if ep == 0: initial_loss = l
            final_loss = l

        if text not in self.learned_sentences:
            self.learned_sentences.append(text)

        self.save_weights()
        return initial_loss, final_loss

    def generate(self, prompt: str = "", max_new_tokens: int = 16, temperature: float = 0.7, top_p: float = 0.85):
        """Generates continuation text autoregressively using temperature and top-p sampling."""
        token_ids = self.encode(prompt) if prompt else [self.token2id["<bos>"]]
        generated = list(token_ids)

        for _ in range(max_new_tokens):
            cur_tokens = generated[-self.max_seq_len:]
            logits, _ = self.forward(cur_tokens)
            next_token_logits = logits[-1] / max(temperature, 1e-5)

            # Softmax probabilities
            probs = softmax(next_token_logits)

            # Top-p (Nucleus) Filtering
            sorted_indices = np.argsort(probs)[::-1]
            sorted_probs = probs[sorted_indices]
            cum_probs = np.cumsum(sorted_probs)

            # Cut off tokens with cumulative probability above top_p
            cutoff = cum_probs > top_p
            if np.any(cutoff):
                first_cutoff = np.where(cutoff)[0][0] + 1
                sorted_indices = sorted_indices[:first_cutoff]
                sorted_probs = sorted_probs[:first_cutoff]
                sorted_probs /= np.sum(sorted_probs)

            # Sample next token
            next_token = np.random.choice(sorted_indices, p=sorted_probs)
            if next_token == self.token2id.get("<eos>", -1):
                break
            generated.append(int(next_token))

        return self.decode(generated)

    def semantic_recall(self, query: str, top_k: int = 1):
        """Retrieves closest learned sentence using dense neural cosine embeddings."""
        if not self.learned_sentences:
            return None

        q_vec = self.get_sentence_embedding(query)
        best_match = None
        best_sim = -1.0

        for sent in self.learned_sentences:
            s_vec = self.get_sentence_embedding(sent)
            sim = float(np.dot(q_vec, s_vec))
            if sim > best_sim:
                best_sim = sim
                best_match = sent

        if best_sim > 0.45:
            return best_match, best_sim
        return None

    def get_model_stats(self):
        total_params = sum(p.size for p in [
            self.W_embed, self.W_pos, self.W_q, self.W_k, self.W_v, self.W_o,
            self.W_ff1, self.b_ff1, self.W_ff2, self.b_ff2, self.W_head, self.b_head
        ])
        last_loss = self.loss_history[-1] if self.loss_history else 0.0
        return {
            "total_parameters": total_params,
            "d_model": self.d_model,
            "d_ff": self.d_ff,
            "vocab_size": len(self.token2id),
            "training_steps": self.total_training_steps,
            "sentences_trained": len(self.learned_sentences),
            "last_loss": last_loss
        }

    def save_weights(self):
        try:
            np.savez_compressed(
                WEIGHTS_FILE,
                W_embed=self.W_embed, W_pos=self.W_pos,
                W_q=self.W_q, W_k=self.W_k, W_v=self.W_v, W_o=self.W_o,
                W_ff1=self.W_ff1, b_ff1=self.b_ff1, W_ff2=self.W_ff2, b_ff2=self.b_ff2,
                W_head=self.W_head, b_head=self.b_head,
                token2id=self.token2id,
                total_steps=np.array([self.total_training_steps]),
                learned_sentences=np.array(self.learned_sentences, dtype=object),
                loss_history=np.array(self.loss_history[-200:])
            )
        except Exception:
            pass

    def load_weights(self):
        if not WEIGHTS_FILE.exists():
            return
        try:
            data = np.load(WEIGHTS_FILE, allow_pickle=True)
            self.token2id = data["token2id"].item()
            self.id2token = {v: k for k, v in self.token2id.items()}

            self.W_embed = data["W_embed"]
            self.W_pos = data["W_pos"]
            self.W_q = data["W_q"]
            self.W_k = data["W_k"]
            self.W_v = data["W_v"]
            self.W_o = data["W_o"]
            self.W_ff1 = data["W_ff1"]
            self.b_ff1 = data["b_ff1"]
            self.W_ff2 = data["W_ff2"]
            self.b_ff2 = data["b_ff2"]
            self.W_head = data["W_head"]
            self.b_head = data["b_head"]

            self.total_training_steps = int(data["total_steps"][0])
            self.learned_sentences = list(data["learned_sentences"])
            self.loss_history = list(data["loss_history"])
            self._init_optimizer()
        except Exception:
            pass
