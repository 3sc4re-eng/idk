# ACE design references

Reviewed selected menu frames and narration, rather than inferring hidden algorithms from demonstrations.

- [Sensory showcase, 8:59 onward](https://www.youtube.com/watch?v=IDtddCUXwjE&t=539s): movable ESP elements, individual appearance popups, category overrides, and explicit hardware diagnostics. Relevant to the shared builder and future CS layout.
- [RedEngine showcase](https://www.youtube.com/watch?v=LQNYm-7rwA0): compact control groups, global/per-weapon settings and linked-axis smoothing presentation. Exact internal movement algorithm is not visible.
- [Zorvex showcase, 3:45 onward](https://www.youtube.com/watch?v=-mOVAYNy_JA&t=225s): separate horizontal/vertical smoothing, reaction timing, selectable model regions and configurable region radii. Region radius is not evidence of manual dead-zone behaviour.
- [Original SigmaDrift source](https://github.com/ck0i/SigmaDrift): a point-to-point path generator with deliberate stochastic effects. ACE retains continuous feedback and caps; its optional deterministic bell envelope is not a port of the full algorithm. No detection-safety claim is made.

Pending design ideas are not advertised as completed functionality. Future CS gameplay integration remains separate from these menu references.
