# Roadmap

1. Replace simulator stub with C++ batch runner outputs (`episodes.jsonl`).
2. Add feature masks for missing attributes and type embeddings per unit category.
3. Upgrade model to DeepSets -> Set Transformer (self-attention + cross-attention).
4. Add symmetry loss by swapping red/blue and enforcing mirrored predictions.
5. Add curriculum schedule from simple scenarios (1v1) to mixed larger formations.
6. Export best model to ONNX and wrap inference API with FastAPI.

