# Reinforcement Learning: Concepts, Algorithms, and Applications in Game AI

## Section A. First Concepts and Basic Algorithms
### 0. Mathematical Notation and Preliminaries
To establish a rigorously formal foundation for the subsequent analysis of **Reinforcement Learning** (RL), the mathematical notations and definitions utilized throughout this report must be explicitly codified. Reinforcement Learning operates fundamentally on the principle of sequential decision-making under uncertainty, modeled mathematically as a **Markov Decision Process** (MDP). 

The architecture of an MDP, and the subsequent algorithms designed to solve it, rely on the following established formalisms. \
The environment is modeled as a measurable **State Space** $\mathcal{S}$, representing all possible configurations the system can occupy. In discrete board games such as Chess or Go, this space is finite but astronomically large (e.g., $10^{43}$ and $10^{170}$ states, respectively). A specific state at time $t$ is denoted $s_t \in \mathcal{S}$. \
The agent interacts with this environment via an **Action Space** $\mathcal{A}$, which may be *discrete* (as in placing a tile in Hive) or *continuous* (as in robotic joint torques). A specific action executed at time $t$ is denoted $a_t \in \mathcal{A}$.\
The environmental dynamics are governed by a **Transition Probability Kernel** $\mathcal{P}$, formally defined as $P(s_{t+1} | s_t, a_t)$. This function dictates the probability distribution over subsequent states given the current state and the chosen action, encapsulating the stochasticity of the environment. \
The feedback mechanism is provided by the **Reward Function** $\mathcal{R}$, a scalar mapping $\mathcal{R}: \mathcal{S} \times \mathcal{A} \times \mathcal{S} \to \mathbb{R}$. The immediate reward obtained at time $t$ is denoted $r_t$. \
To ensure that infinite-horizon returns remain mathematically bounded, a **Discount Factor** $\gamma \in [0,1]$ is introduced. \
The behavioral mechanism of the agent is the **Policy** $\pi$. A stochastic policy is defined as a conditional probability distribution $\pi(a_t | s_t) = P(A_t = a_t | S_t = s_t)$, mapping states to a distribution over actions. A purely deterministic policy is denoted $\mu(s_t)$. \
The ultimate optimization objective of the RL agent is the **Expected Discounted Return** $J(\pi)$, defined as $J(\pi) = \mathbb{E}_{\tau \sim \pi} [\sum_{t=0}^{\infty} \gamma^t r_t]$, where $\tau$ represents a trajectory of states, actions, and rewards. 

To evaluate policies, two core functions are defined. \
The **State-Value Function** $V^\pi(s)$ measures the expected return starting from a specific state $s$ and following policy $\pi$ indefinitely: $V^\pi(s) = \mathbb{E}_\pi [\sum_{t=0}^{\infty} \gamma^t r_t | S_0 = s]$. \
The **Action-Value Function** $Q^\pi(s, a)$ measures the expected return starting from state $s$, taking action $a$, and subsequently following policy $\pi$: $Q^\pi(s, a) = \mathbb{E}_\pi [\sum_{t=0}^{\infty} \gamma^t r_t | S_0 = s, A_0 = a]$. \
Finally, a derived measure called **Advantage Function** $A^\pi(s, a) = Q^\pi(s, a) - V^\pi(s)$ quantifies the relative utility of a specific action compared to the baseline expected value of the state. 

These formal definitions serve as the absolute mathematical grammar for the classical algorithms, deep neural approximations, and advanced game-theoretic paradigms explored in this comprehensive report.

### 1. The Axiomatic Foundation: The Markov Decision Process (MDP)
The entirety of modern Reinforcement Learning is anchored upon the mathematical architecture of the Markov Decision Process (MDP). 

Originating from operations research in the 1950s, the MDP provides a formally defined tuple $\mathcal{M} = \langle \mathcal{S}, \mathcal{A}, \mathcal{P}, \mathcal{R}, \gamma, \rho_0 \rangle$ that models the stochastic interaction between an agent and an environment. \
The MDP framework is specifically designed to provide a simplified yet robust representation of the critical elements of artificial intelligence: understanding cause and effect, managing uncertainty, and pursuing explicit, mathematically codified goals. 

The core philosophical and mathematical assumption underpinning this framework is the **Markov Property**.\
This foundational principle dictates that the future evolution of the process depends exclusively on the present state and action, entirely independent of the historical sequence of events that preceded it. Mathematically, this is expressed as an equality of conditional probabilities:
$$\mathbb{P}(S_{t+1} = s', R_{t+1} = r \mid S_t, A_t) = \mathbb{P}(S_{t+1} = s', R_{t+1} = r \mid S_t, A_t, S_{t-1}, A_{t-1}, \dots, S_0, A_0)$$

This assumption is a mathematical necessity that renders sequential decision-making tractable. \
By ensuring that the state signal $S_t$ encapsulates all necessary historical information, the necessity to condition policies on exponentially growing historical sequences is bypassed. \
The probability flow of this process through time is dictated by a discrete-time analog of the Chapman-Kolmogorov equations, which recursively computes the probability of transitioning from one state to another over multiple steps by marginalizing over all possible intermediate states. 

Understanding the MDP requires a nuanced appreciation of its optimization objective: maximizing the Expected Discounted Return, $J(\pi)$. \
The discount factor $\gamma$ mathematically ensures that the infinite series of rewards converges to a finite bound, provided the reward signal is bounded. However, it also dictates the agent's strategic time horizon. \
As $\gamma \to 0$, the agent becomes entirely myopic, optimizing solely for immediate gratification; as $\gamma \to 1$, the agent becomes far-sighted, weighting long-term strategic positioning equally with immediate tactical gains. \
In episodic games like Chess or Go, where the only non-zero reward occurs at the terminal state (win/loss/draw), a $\gamma$ value of 1 (or extremely close to it) is necessary to propagate the terminal signal back through the sequence of moves. 

The mathematical proof that a deterministic optimal policy $\pi^*$ exists for any finite MDP—a policy that strictly dominates or equals all other policies for all possible states—is one of the foundational triumphs of stochastic dynamic programming. 

The classical MDP formulation assumes full observability. \
However, this foundational chapter must acknowledge its extensions, notably the **Partially Observable Markov Decision Process** (POMDP): in complex imperfect-information games such as Poker, the agent receives an observation $o_t$ rather than the true state $s_t$. \
In such scenarios, the Markov property is violated from the agent's perspective, necessitating the maintenance of a belief state—a probability distribution over the underlying MDP states. \
The transition from MDPs to POMDPs highlights the fragility of the Markov assumption and necessitates the advanced regret-matching algorithms explored in later chapters of this report.


### 2. Value Functions and The Bellman Equations
If the MDP is the stage, value functions are the primary analytical instruments used to navigate it. \
*Value functions* quantify the long-term utility of states ($V^\pi(s)$) and state-action pairs ($Q^\pi(s, a)$). \
In the context of perfect-information games, the state-value function is synonymous with the concept of a board evaluation function. \
While early Chess engines like Deep Blue relied on static, human-crafted evaluation functions (e.g., material advantage, pawn structure), modern RL attempts to learn the true mathematical expectation of victory directly. 

The computational genius of Reinforcement Learning relies on the recursive decomposition of these expected returns, a breakthrough encapsulated by the **Bellman Equations**. \
The **Bellman Expectation Equation** provides a recursive consistency condition for any given policy $\pi$. \
It dictates that the value of a current state must exactly equal the expected immediate reward plus the discounted value of all possible subsequent states, weighted by their transition probabilities:
$$V^\pi(s) = \sum_{a \in \mathcal{A}} \pi(a|s) \sum_{s', r} P(s', r | s, a) \left[ r + \gamma V^\pi(s') \right]$$

While the expectation equation evaluates a specific, fixed behavior, the goal of Reinforcement Learning is global optimization. \
This requires the formulation of the Bellman Optimality Equation, a non-linear system that defines the unique optimal value functions. \
The optimal action-value function recognizes that an optimal agent will always select the action that maximizes the expected return. Therefore, it is defined recursively using a maximization operator:
$$Q^*(s, a) = \sum_{s', r} P(s', r | s, a) \left[ r + \gamma \max_{a'} Q^*(s', a') \right]$$

The theoretical guarantee that this recursive system can be solved relies heavily on functional analysis, specifically the Banach Fixed-Point Theorem. \
We define the Bellman Optimality Operator $\mathcal{T}^*$ such that it applies the right-hand side of the optimality equation to an arbitrary value function. \
It can be rigorously proven that $\mathcal{T}^*$ is a $\gamma$-contraction mapping under the $L_\infty$ supremum norm. Specifically, for any two arbitrary Q-functions $Q_1$ and $Q_2$:
$$||\mathcal{T}^*Q_1 - \mathcal{T}^*Q_2||_\infty \leq \gamma ||Q_1 - Q_2||_\infty$$

Because the discount factor $\gamma < 1$, repeated applications of the operator $\mathcal{T}^*$ will systematically reduce the mathematical distance between any initial value function and the unique fixed point $Q^*$. \
This rigorous guarantee ensures that iterative algorithms possess a stable, inescapable convergence point. \
Once $Q^*$ is obtained, extracting the optimal policy becomes a trivial operation of acting greedily with respect to the optimal values. \
However, obtaining exact solutions is computationally expensive. \
While the linear Expectation Equation can be solved via matrix inversion with cubic time complexity $\mathcal{O}(|\mathcal{S}|^3)$, the non-linear Optimality Equation cannot be solved directly and requires iterative approximation. \
When applied to the combinatorial explosion of board games, such exact methods are impossible, necessitating the approximation methods detailed hereafter.

### 3. Classical Solutions: Dynamic Programming (DP)
Dynamic Programming (DP) refers to a collection of exact algorithmic methodologies designed to compute optimal policies under the strict condition that the agent possesses a perfect, mathematically complete model of the environment. \
This implies exact knowledge of the transition probabilities $\mathcal{P}$ and the reward function $\mathcal{R}$. \
While this assumption is rarely met in chaotic real-world tasks, DP algorithms form the conceptual and mathematical backbone for all subsequent reinforcement learning techniques. \
Furthermore, in discrete deterministic games, the transition model is the rules of the game itself, making DP highly relevant in limited contexts. 

The two primary paradigms within Dynamic Programming are **Policy Iteration** and **Value Iteration**. 

Policy Iteration operates by decoupling the evaluation of a policy from its improvement. \
The algorithm systematically alternates between two distinct mathematical phases. 
1. First, in the *Policy Evaluation* phase, the state-value function $V^\pi$ for an arbitrary initial policy $\pi$ is computed. \
    Because the policy is fixed, the non-linear maximization operator is removed, and the Bellman Expectation Equation collapses into a system of $|\mathcal{S}|$ linear equations. \
    While this system can theoretically be solved directly, it is typically approximated iteratively to avoid cubic computational costs. 
2. Second, the *Policy Improvement* phase applies the **Policy Improvement Theorem**. \
    This theorem mathematically guarantees that creating a new policy $\pi'$ that acts greedily with respect to the value function of the old policy $\pi$ will yield a policy that is strictly better than or equal to $\pi$. \
3. The cycle between the two phases repeats iteratively until the policy stops changing, mathematically indicating convergence to the global optimum.

Conversely, Value Iteration recognizes that waiting for exact convergence in the Policy Evaluation step is computationally wasteful. \
Instead, it effectively truncates the evaluation step to a single sweep, updating the value function by applying the non-linear Bellman Optimality Operator directly:
$$V_{k+1}(s) = \max_{a} \sum_{s', r} P(s', r | s, a) \left[ r + \gamma V_k(s') \right]$$
Due to the contraction mapping property discussed in the previous chapter, the sequence of value functions $V_k$ is mathematically guaranteed to converge to $V^*$ as $k \to \infty$. 

Despite their mathematical elegance and absolute guarantees of optimality, DP algorithms are famously crippled by Richard Bellman's "Curse of Dimensionality." \
Because they require sweeping across the entire state space during every single iteration, their computational and memory requirements scale exponentially with the number of state variables. \
For a game like Chess, a single sweep over the estimated $10^{43}$ state space would require computing power far exceeding the physical limits of the universe. \
However, DP is heavily utilized in game sub-domains; for instance, Chess endgame tablebases (such as the Lomonosov tablebases) perfectly solve the game for all positions with seven or fewer pieces using exact retrograde DP analysis. \
For the full game, however, overcoming these limitations paved the way for model-free learning techniques.

### 4. Model-Free Learning: TD, Q-Learning, and SARSA
When the environment model ($\mathcal{P}$ and $\mathcal{R}$) is unknown, or the state space is too vast for explicit sweeps, the agent must learn the optimal policy purely through empirical interaction. \
This paradigm is known as **Model-Free Learning**. \

At the absolute heart of this paradigm lies **Temporal Difference** (TD) learning, an algorithmic breakthrough that elegantly bridges the gap between *Monte Carlo* (MC) sampling and *Dynamic Programming* (DP). \
While Monte Carlo methods must wait until the absolute end of an episode to calculate the unbiased return $G_t$, and DP uses a perfect model to bootstrap computations without sampling, TD learning synthesizes these approaches. \
It samples the environment (like MC) but bootstraps from its own current value estimates (like DP) after a single step. 

This creates a fundamental trade-off: TD introduces bias into the update (due to reliance on the current, imperfect estimate $V(s_{t+1})$) to drastically reduce variance (by not relying on the full stochastic chain of future rewards). \
The update is driven by the Temporal Difference Error ($\delta_t$), which quantifies the surprise between the current estimate and the one-step reality:

$$\delta_t = R_{t+1} + \gamma V(S_{t+1}) - V(S_t)$$
$$V(S_t) \leftarrow V(S_t) + \alpha \delta_t$$

This prediction principle is extended to control via two historically monumental algorithms: **SARSA** and **Q-Learning**. \
The distinction between them lies in the relationship between the Behavior Policy (used to generate experience) and the Target Policy (being learned).

**SARSA** (State-Action-Reward-State-Action) is an on-policy algorithm, meaning the behavior and target policies are identical. \
It updates its estimates based on the action $A_{t+1}$ actually chosen by the current policy (including exploratory moves). \
Its update rule is:
$$Q(S_t, A_t) \leftarrow Q(S_t, A_t) + \alpha \left[ R_{t+1} + \gamma Q(S_{t+1}, A_{t+1}) - Q(S_t, A_t) \right]$$
Because SARSA anticipates the agent's actual future behavior, it learns a "safer" policy that accounts for the risk of exploration. \
SARSA algorithm has to choose the "next" action $A_{t+1}$. The common way to do this is to use an $\epsilon$-greedy policy, which selects the action with the highest estimated value with probability $1-\epsilon$ and a random action with probability $\epsilon$. \
This allows to balance two key aspects of learning: exploitation (choosing the best-known action) and exploration (trying new actions to discover their value). \
In order to converge to the optimal policy, SARSA must satisfy the **GLIE** (Greedy in the Limit with Infinite Exploration) property, which ensures that the exploration rate $\epsilon$ decays to zero asymptotically, allowing the policy to become greedy with respect to the learned Q-values.


**Q-Learning** (Watkins, 1989) is an off-policy algorithm. \
It decouples the behavior policy from the target policy. \
While the agent may explore using a stochastic policy (e.g., $\epsilon$-greedy), the algorithm learns the value of the optimal greedy policy $\pi^*$ by assuming optimal future play. \
This is mathematically achieved by utilizing the maximization operator over the next state's actions, regardless of the action actually taken:

$$Q(S_t, A_t) \leftarrow Q(S_t, A_t) + \alpha \left[ R_{t+1} + \gamma \max_{a'} Q(S_{t+1}, a') - Q(S_t, A_t) \right]$$

Q-Learning is an *off-policy* algorithm: it does not need to be GLIE in order to converge. Howver, it relies on the *Stochastic Approximation Theory*: the learning rate $\alpha_t$ must satisfy the **Robbins-Monro Conditions**: $$\sum \alpha_t = \infty$$, to overcome initial conditions, and $$\sum \alpha_t^2 < \infty$$, to dampen variance and ensure stability. 

Historically, the power of these methods was proven by Gerald Tesauro’s TD-Gammon (1992). This agent utilized TD($\lambda$), a method that uses eligibility traces to interpolate between one-step TD and full Monte Carlo returns, allowing credit to be assigned to decisions made many steps in the past. By combining TD($\lambda$) with a non-linear function approximator (a neural network) in a state space of $10^{20}$, TD-Gammon achieved superhuman performance, effectively serving as the precursor to the Deep Reinforcement Learning revolution.

### 5. Deep Reinforcement Learning (DRL)
As state spaces expand into high-dimensional sensory inputs —such as the raw pixel streams of Atari video games or the complex continuous kinematics of modern robotics— the tabular representation of $Q(s, a)$ becomes computationally intractable. 

eep Reinforcement Learning (DRL) circumvents this dimensional curse by utilizing deep neural networks as powerful, non-linear function approximators of 

$$Q(s, a; \theta) \approx Q^*(s, a)$$

where $\theta$ denotes the network's weights.

However, substituting neural networks directly into Q-learning introduces severe mathematical instability. \
As formally identified by RL theorists, the field is vulnerable to the **Deadly Triad**: the simultaneous combination of *function approximation*, *bootstrapping* (updating based on estimates rather than actual returns), and *off-policy training*. \
This combination reliably causes value divergence because neural network gradients become highly correlated over sequential temporal data, and non-stationary targets create chaotic feedback loops that destabilize the optimization landscape. 

The seminal Deep Q-Networks (DQN) architecture, introduced by Mnih et al. (2015), famously achieved human-level performance across a vast suite of Atari games directly from raw pixel inputs. \
It mitigated the instabilities of the Deadly Triad through two primary mathematical and architectural innovations. 
- First, DQN introduced the **Experience Replay Buffer**. \
Instead of learning from sequential, highly correlated transitions, the agent stores millions of experience tuples $e_t = \langle s_t, a_t, r_t, s_{t+1} \rangle$ into a large dataset $\mathcal{D}$. \
During the training phase, the algorithm samples randomized mini-batches uniformly from this buffer. \
This critical mechanism breaks temporal correlations, satisfying the independent and identically distributed (i.i.d.) assumption required by stochastic gradient descent (SGD) optimization. \
Furthermore, it improves data efficiency by allowing rare, high-value experiences to be reused multiple times.
- Second, the architecture implemented **Target Networks**. \
In standard Q-learning, the target $$y = r + \gamma \max_{a'} Q(s', a'; \theta)$$ is non-stationary, shifting immediately whenever the parameters $\theta$ are updated. \
DQN mitigates this by computing the target using a separate, slowly updating target network with frozen parameters $\theta^-$. 

The loss function is defined as the expectation of the squared Temporal Difference error over the mini-batch:$$\mathcal{L}(\theta) = \mathbb{E}_{\langle s, a, r, s' \rangle \sim \mathcal{D}} \left[ \left( r + \gamma \max_{a'} Q(s', a'; \theta^-) - Q(s, a; \theta) \right)^2 \right]$$

In practice, implementations often utilize the Huber Loss (Smooth L1) instead of the squared error to prevent gradient explosion when the error term is large. 

By freezing $\theta^-$ and only periodically synchronizing it with the main network $\theta$ (e.g., every $C$ steps), the optimization objective remains strictly stationary between updates, effectively dampening divergence. \
Further advancements, such as **Double DQN** (which decouples action selection from action evaluation to prevent overestimation bias) and **Prioritized Experience Replay** (which samples experiences based on the magnitude of their TD error), have since solidified DRL as the standard framework for high-dimensional control tasks.

### 6. Policy Gradient Methods
While value-based methods implicitly derive a policy by acting greedily over estimated Q-values (often requiring discrete inputs), **Policy Gradient** (PG) methods directly parameterize the policy itself as a differentiable function $\pi_\theta(a|s)$, such as a neural network.  \
The parameters $\theta$ are optimized through direct gradient ascent on the expected return $J(\theta)$. 

This approach holds profound mathematical and practical advantages: it natively handles high-dimensional or continuous action spaces (where computing $\max_a Q$ is intractable) and can learn stochastic policies capable of solving problems with partial observability or adversarial dynamics.

The mathematical cornerstone of this paradigm is the **Policy Gradient Theorem**. \
This theorem rigorously proves that the gradient of the performance objective with respect to the policy parameters does not depend on the derivative of the state distribution, despite the state distribution being a function of the policy. \
Utilizing the **Log-Derivative Trick** — which leverages the identity $\nabla_\theta \pi_\theta = \pi_\theta \nabla_\theta \log \pi_\theta$ — the gradient is formulated elegantly as an expectation:

$$\nabla_\theta J(\theta) = \mathbb{E}_{\pi_\theta} \left[ \nabla_\theta \log \pi_\theta(a_t | s_t) Q^{\pi_\theta}(s_t, a_t) \right]$$

In the seminal **REINFORCE** algorithm, the $Q$-value is estimated using the unbiased empirical Monte Carlo return $G_t$. \
However, raw empirical returns introduce high variance, severely slowing convergence. \
To mitigate this, advanced methods subtract a state-dependent baseline $b(s_t)$, commonly the value function $V(s_t)$. \
Crucially, because the baseline is independent of the action $a_t$, it reduces variance without introducing bias.

This yields the **Advantage Function** $A(s_t, a_t) = Q(s_t, a_t) - V(s_t)$, forming the basis of Actor-Critic architectures. \
In these systems, an "Actor" optimizes the policy $\pi_\theta$ while a "Critic" minimizes the error of the value estimate $V_\phi$.\
To prevent "catastrophic forgetting"—where a single excessively large gradient step pushes the policy into a degenerate region of parameter space—modern algorithms employ strict trust regions. 

**Proximal Policy Optimization** (PPO), the current industrial standard, enforces this stability by constraining the update. \
PPO defines a probability ratio 

$$r_t(\theta) = \frac{\pi_\theta(a_t|s_t)}{\pi_{\theta_{old}}(a_t|s_t)}$$

and optimizes a clipped surrogate objective function:
$$\mathcal{L}^{CLIP}(\theta) = \mathbb{E}_t \left[ \min\left(r_t(\theta)A_t, \text{clip}(r_t(\theta), 1-\epsilon, 1+\epsilon)A_t\right) \right]$$

This clipped objective mathematically penalizes policy updates that shift the probability ratio $r_t(\theta)$ too far from 1, ensuring monotonic, stable improvement. \
Crucially, PPO achieves this with first-order optimization, avoiding the massive computational overhead of calculating the inverse Fisher Information Matrix required by second-order algorithms like Trust Region Policy Optimization (TRPO).

### 7. Planning and Search: MCTS and AlphaGo
In environments possessing a perfectly known simulator—primarily deterministic, perfect-information combinatorial board games like Chess, Shogi, and Go—pure model-free RL is drastically outperformed by combining neural network approximations with explicit forward search. 

The zenith of this integrated approach is *Monte Carlo Tree Search* (MCTS) paired with deep neural networks.

MCTS builds a lookahead search tree iteratively. \
Each node $s$ in the tree represents a state, storing two critical statistics: the visit count $N(s)$ and the total accumulated value $W(s)$ (or mean value $Q(s) = W(s)/N(s)$). \
The algorithm proceeds through four distinct, mathematically defined phases per iteration:
1. **Selection**: Starting from the root node $s_0$, the algorithm traverses down the tree by selecting the child node that maximizes a specific selection policy (typically UCB1, detailed below). \
This selection process is repeated recursively until a leaf node is reached (a node that has not yet been fully expanded).

2. **Expansion**: Unless the leaf node represents a terminal state of the game, one or more child nodes are added to the tree to represent available future actions $a \in \mathcal{A}(s)$.

3. **Simulation** (Rollout): In classical MCTS, the algorithm performs a Monte Carlo simulation from the new leaf node. A "rollout policy" (often a fast, random, or heuristic-based policy) plays the game to completion to generate a terminal reward $z$ (e.g., +1 for win, -1 for loss). As detailed later, AlphaZero replaces this stochastic rollout with a deterministic Neural Network evaluation.

4. **Backpropagation**: The result $z$ of the simulation is propagated backward up the tree, from the leaf to the root. \
For every node traversed during the Selection phase, the visit count is incremented ($N(s) \leftarrow N(s) + 1$) and the total value is updated ($W(s) \leftarrow W(s) + z$). \
This ensures that the statistics at the root eventually reflect the long-term value of the downstream paths.

The mathematical engine of the Selection phase is the **Upper Confidence Bound** (UCB1) algorithm. \
This formulation treats the selection of a child node as a Multi-Armed Bandit problem. \
We must choose which "arm" (child node) to pull to maximize reward, balancing the need to play the move with the highest current win rate (Exploitation) against the need to sample moves with few visits to ensure we haven't missed a hidden optimal strategy (Exploration). \
For a parent node with $N$ total visits, the algorithm selects the child $j$ that maximizes:
$$\text{UCB1}_j = \underbrace{\bar{X}_j}_{\text{Exploitation}} + \underbrace{C \sqrt{\frac{2 \ln N}{n_j}}}_{\text{Exploration}}$$

- $\bar{X}_j$ (Mean Action Value) identifies the average reward obtained from child $j$ so far. \
This term drives the algorithm to favor actions that have historically performed well.

- $\sqrt{\frac{2 \ln N}{n_j}}$ (Uncertainty Term): Derived from the Hoeffding Inequality, this term represents the upper confidence bound of the reward estimate. \
 As the parent is visited more ($N$ increases) but the child is neglected ($n_j$ stays small), this term grows logarithmically, eventually forcing the algorithm to explore the neglected child.
 
 - $C$: An exploration constant that balances the two terms.
 
The AlphaGo architecture and its successor AlphaZero revolutionized this paradigm. \
While classical MCTS relies on random rollouts (Phase 3), AlphaZero eliminates the simulation phase entirely. \
Instead, it utilizes a deep neural network $f_\theta(s) = (\mathbf{p}, v)$ to evaluate the leaf node directly. \
AlphaZero modifies the standard UCB1 formula into a variant known as **PUCT** (Polynomial Upper Confidence Trees), which incorporates the prior probability $P(s,a)$ emitted by the policy network to guide the search more efficiently than random exploration:
$$a_t = \arg\max_{a} \left( Q(s,a) + c_{puct} P(s,a) \frac{\sqrt{\sum_b N(s,b)}}{1+N(s,a)} \right)$$

Here, the neural network's prior $P(s,a)$ effectively prunes the search space by biasing the exploration term towards moves the network already believes are strong (human intuition), while $Q(s,a)$ provides the empirical ground truth from the search (verification). \
The self-play loop iteratively trains the unified ResNet architecture, minimizing a composite loss function $\mathcal{L}(\theta)$ that combines the Mean Squared Error of the value head and the Cross-Entropy of the policy head:
$$\mathcal{L}(\theta) = (z - v)^2 - \boldsymbol{\pi}^T \log \mathbf{p} + c ||\theta||^2$$
- $z$: The actual game outcome ($+1, -1, 0$).
- $v$: The network's predicted value.$\boldsymbol{\pi}$: The improved policy distribution generated by the MCTS visit counts (acting as the training target).
- $\mathbf{p}$: The network's raw policy logits. 

By using the search tree as a highly localized, massively powerful policy improvement operator, AlphaZero achieved superhuman mastery in structurally diverse games (defeating Stockfish in Chess, Elmo in Shogi, and AlphaGo Zero in Go) within mere hours.


## Section B. Frontiers and Open Problems
### 8. The Frontier: RLHF for Large Language Models
While game environments provide strict structural rules and unambiguous terminal states, applying Reinforcement Learning to open-ended human alignment requires complex, subjective reward modeling. 

**Reinforcement Learning from Human Feedback** (RLHF) defines the modern frontier for aligning Large Language Models (LLMs) with nuanced human intent. \
The RLHF pipeline operates in three distinct mathematical phases. 
1. Following Supervised Fine-Tuning (SFT) on high-quality instruction datasets to create a reference policy $\pi_{\text{ref}}$, the system learns a **Reward Model** (RM) $r_\phi(x, y)$ using human preference data. 
2. Given a prompt $x$ and two candidate responses $(y_w, y_l)$ where $y_w$ is preferred over $y_l$, the RM is trained to assign a higher scalar score to the preferred completion. This is mathematically formalized using the **Bradley-Terry model**, which treats the probability of human preference as a sigmoid function of the difference in latent reward scores:
$$P(y_w \succ y_l | x) = \sigma(r_\phi(x, y_w) - r_\phi(x, y_l))$$
3. The parameters $\phi$ of the reward model are optimized by minimizing the negative log-likelihood of the preference data:
$$\mathcal{L}(\phi) = - \mathbb{E}_{(x, y_w, y_l) \sim \mathcal{D}} \left[ \log \sigma(r_\phi(x, y_w) - r_\phi(x, y_l)) \right]$$

Once trained, the RM acts as a surrogate environment, providing the scalar reward signal required for standard RL algorithms. \
The LLM acts as the policy $\pi_\theta$, and the optimization is typically executed via Proximal Policy Optimization (PPO). 

Crucially, the objective function must be heavily constrained to prevent Reward Hacking—a systemic failure mode where the optimizer exploits adversarial loopholes in the proxy Reward Model to maximize the numerical score while generating syntactically incoherent or repetitive gibberish. \
This is mitigated by appending a Kullback-Leibler (KL) divergence penalty to the reward signal, firmly anchoring the active policy $\pi_\theta$ to the original supervised reference model $\pi_{\text{ref}}$. \
The modified reward $R(x, y)$ used for the PPO update is:
$$R(x, y) = r_\phi(x, y) - \beta \log \frac{\pi_\theta(y|x)}{\pi_{\text{ref}}(y|x)}$$

where $\beta$ is a hyperparameter controlling the strength of the KL penalty. 

While RLHF has enabled unprecedented dialogue capabilities and instruction-following, its reliance on proxy reward models stands in stark contrast to the mathematical purity of terminal game states in Chess or Go. \
The vulnerability to mode collapse and the unreliability of reward modeling continue to pose rigorous open problems in the alignment space, indicating that RL applied to generative text remains mathematically less tractable than RL applied to zero-sum combinatorial games.

### 9. Distributional Reinforcement Learning
Traditional Reinforcement Learning optimizes exclusively for the mathematical expectation of cumulative returns $\mathbb{E}[G_t]$. \
This implicitly assumes a risk-neutral agent, collapsing all stochasticity—such as dice rolls, card draws, or adversarial behavior—into a single scalar mean. \
However, the true return of an environment is inherently a random variable $Z$ characterized by complex variance, skewness, and multimodality.

Distributional Reinforcement Learning (DistRL) fundamentally reformulates the Bellman equation to model the complete probability distribution of returns. \
The mathematical core of this domain is the **Distributional Bellman Operator**. \
Instead of mapping scalar values to scalar values, this operator maps entire return distributions to return distributions. 

Let $Z(x,a)$ represent the random variable of the return. \
The distributional Bellman equation is defined as an equality in distribution:
$$Z(x,a) \stackrel{D}{=} R(x,a) + \gamma Z(X', A')$$

The seminal algorithmic instantiation of this theory is C51 (Categorical 51-Atom DQN). \
C51 parameterizes the continuous return distribution as a discrete categorical distribution over a fixed support of $N$ atoms $\{z_1, \dots, z_N\}$. \
However, the Bellman update $R + \gamma z_i$ shifts and scales these atoms, resulting in a target distribution whose support is disjoint from the original fixed atoms. \
C51 solves this via a heuristic Projection Step $\Phi$, which redistributes the probability mass of the updated Bellman target onto the original support atoms using linear interpolation. \
The network is then trained by minimizing the Kullback-Leibler (KL) divergence between the predicted distribution and the projected target:
$$\mathcal{L}_{C51}(\theta) = D_{KL}(\Phi \mathcal{T} Z_\theta || Z_\theta)$$

To overcome the disjoint support issues of C51, Quantile Regression DQN (QR-DQN) inverts the parameterization. \
Instead of fixing locations (atoms) and learning probabilities, QR-DQN fixes the probabilities (quantiles) and learns the locations. \
Theoretically, this approach leverages the Wasserstein Metric (Earth Mover's Distance), $W_1$. \
Unlike KL divergence, the Wasserstein metric is a valid distance metric between disjoint distributions, and it can be rigorously proven that the Distributional Bellman Operator is a contraction mapping under $W_1$. \
QR-DQN approximates this minimization using the Quantile Huber Loss.

Implicit Quantile Networks (IQN) push this frontier further by utilizing a fully parameterized continuous quantile function $Z_\tau(x,a)$, where $\tau \sim U([0,1])$. \
This allows the network to query the return value at any arbitrary probability $\tau$. \
By applying non-linear Distortion Risk Measures $\beta(\tau)$ to the sampling distribution of $\tau$, the agent can dynamically shift its policy between risk-averse (weighing worst-case outcomes heavily) and risk-seeking behaviors, granting unprecedented flexibility in stochastic domains.

### 10. Offline Reinforcement Learning: Conservative and Implicit Q-Learning
Deploying RL in the real world is often stifled by the strict requirement for active environmental exploration; a medical diagnosis agent or an autonomous vehicle cannot act randomly to gather data without causing catastrophic harm. 

Offline Reinforcement Learning attempts to learn an optimal policy strictly from a static, pre-recorded dataset $\mathcal{D}$, fundamentally eliminating active environment interaction. \
In the domain of games, this equates to training an agent to superhuman levels relying solely on databases of historical Grandmaster games, without ever allowing the agent to play test games against itself. 

The catastrophic mathematical failure point for standard off-policy algorithms (like DQN) in the offline setting is Extrapolation Error (or Distribution Shift). \
When the Bellman equation computes the target $y = r + \gamma \max_{a'} Q(s', a')$, the maximization operator heavily biases the network toward Out-Of-Distribution (OOD) actions—actions never observed in the dataset. \
Because the network has never been trained on these actions, their Q-values can be erroneously overestimated. \
This overestimation propagates recursively through the Bellman updates, utterly shattering the value function and leading to degenerate policies. 

**Conservative Q-Learning** (CQL) addresses this extrapolation error through an elegant mathematical regularization. \
It explicitly modifies the training objective to artificially penalize the Q-values of actions not present in the dataset, effectively creating a provable lower-bound on the true Q-value. \
The CQL objective minimizes the Q-function under a broad distribution $\mu$ (capturing OOD actions) while maximizing it strictly under the empirical data distribution $\pi_\beta$, balanced against the standard TD error:
$$\mathcal{L}_{CQL}(\theta) = \alpha \left( \mathbb{E}_{s \sim \mathcal{D}, a \sim \mu}[Q_\theta(s,a)] - \mathbb{E}_{s, a \sim \mathcal{D}}[Q_\theta(s,a)] \right) + \frac{1}{2} \mathcal{L}_{TD}(\theta)$$

An alternative, equally profound paradigm is **Implicit Q-Learning** (IQL). 
Instead of directly penalizing OOD actions, IQL modifies the Bellman backup to absolutely guarantee that unseen actions are never queried in the first place. \
It achieves this via Expectile Regression. \
By treating the state-value function $V(s)$ as a random variable, IQL calculates an asymmetric upper expectile $\tau \in (0.5, 1)$ of the Bellman target distribution using the loss function:
$$L_2^\tau(u) = |\tau - \mathbb{I}_{u < 0}| u^2$$
where $u = Q_\theta(s, a) - V_\psi(s)$. 

This formulation pushes the value estimate $V(s)$ toward the maximum of the action-values present within the dataset support, safely approximating the greedy policy without OOD queries. \
Policy extraction is then performed via Advantage-Weighted Regression (AWR). \
The policy $\pi_\phi$ is trained to maximize the likelihood of actions in the dataset, weighted by their estimated advantage:
$$\mathcal{L}_{Actor}(\phi) = \mathbb{E}_{(s, a) \sim \mathcal{D}} \left[ \exp\left(\beta \frac{Q_\theta(s, a) - V_\psi(s)}{\alpha}\right) \log \pi_\phi(a|s) \right]$$

Both CQL and IQL showcase profound mathematical techniques for confining dynamic programming safely within the strict manifold of empirical historical data.

### 11: Multi-Agent Reinforcement Learning (MARL): Value Decomposition Networks
When multiple agents inhabit the exact same environment, the standard MDP mathematical formulation breaks down completely. 

The environment dynamics perceived by one agent become aggressively non-stationary because the other agents are simultaneously learning and altering their behavioral policies. \
In fully cooperative, decentralized settings (such as multi-unit combat in StarCraft II or automated drone swarm scheduling), the problem is formally modeled as a **Decentralized Partially Observable Markov Decision Process** (Dec-POMDP). \
The objective is to maximize a single shared global reward $r_{tot}$ using the paradigm of Centralized Training with Decentralized Execution (CTDE). \
The critical mathematical challenge in CTDE is the Credit Assignment Problem: how does the system deduce an individual agent's specific contribution to the shared team reward? 

This deduction is governed by the **Individual-Global-Max** (IGM) principle. \
IGM asserts that the joint greedy action $\mathbf{u}^*$ selected by the centralized critic must be mathematically equivalent to the set of individual greedy actions selected by the decentralized agents:
$$\arg\max_{\mathbf{u}} Q_{tot}(\boldsymbol{\tau}, \mathbf{u}) = \begin{pmatrix} \arg\max_{u^1} Q_1(\tau^1, u^1) \\ \vdots \\ \arg\max_{u^n} Q_n(\tau^n, u^n) \end{pmatrix}$$

**Value Decomposition Networks** (VDN) satisfy this principle by enforcing a strictly additive structure, assuming the joint action-value $Q_{tot}$ is the simple linear sum of individual utilities $Q_a$:
$$Q_{tot}(\boldsymbol{\tau}, \mathbf{u}) = \sum_{a=1}^n Q_a(\tau^a, u^a)$$

While this satisfies IGM trivially and scales computationally, the linear representation is extremely limited. \
It is utterly unable to capture complex, non-linear strategic interactions where the value of one agent's action depends on the specific action of another (e.g., XOR constraints or synergistic attacks). \
QMIX radically expands this representational capacity by recognizing that the IGM principle does not require linearity; it only requires a monotonicity constraint between the global and local Q-values:
$$\frac{\partial Q_{tot}}{\partial Q_a} \geq 0, \quad \forall a \in \{1, \dots, n\}$$

To enforce this continuous mathematical constraint during deep learning optimization, QMIX employs a sophisticated Hypernetwork architecture. \
The hypernetwork receives the global environment state $s_t$ as input and generates the weights $W$ and biases $b$ for a separate Mixing Network. \
By applying an absolute value operation to the generated weights ($|W|$), the weights remain strictly non-negative. \
This ensures that while the relationship between individual utility $Q_a$ and total utility $Q_{tot}$ can be highly non-linear, an increase in $Q_a$ will always monotonically increase $Q_{tot}$. \
This mathematical guarantee allows QMIX to represent a much richer class of value functions than VDN. 

Advanced variants like Weighted-QMIX (WQMIX) further refine this by introducing weighting schemes to emphasize high-value joint actions, propelling deep MARL to superhuman performance in complex decentralized micro-management environments like the StarCraft Multi-Agent Challenge (SMAC).

| MARL Algorithm | Value Composition | IGM Guarantee | Capacity to Model Interactions |
| ---- | ---- | ---- | ---- |
| VDN | Linear Summation | Yes (Strict Additivity) | Low (No interactions) |
| QMIX | Non-linear Monotonic | Yes (Hypernetwork Weights $\ge 0$) | High (Monotonic interactions) | 
| WQMIX | Weighted Non-linear | Yes (Under specific weighting) | Very High (Non-monotonic approximation) |


### 12: Counterfactual Regret Minimization in Imperfect-Information Games
While MCTS and AlphaZero exhibit absolute dominance in perfect-information games, they fail categorically in Imperfect-Information Games (IIG) like Poker, Liar's Dice, and Fog-of-War strategy games. \
In these environments, the true state of the game is partially hidden (e.g., the opponent's hole cards). 

This necessitates the concept of an **Information Set** ($I$): a collection of game states that are indistinguishable to the acting player based on their observation history. \
Because the exact state is unknown, optimal play demands stochastic mixed strategies to prevent exploitation. 

The premier mathematical framework for reaching a Nash Equilibrium in these environments is **Counterfactual Regret Minimization** (CFR). \
CFR operates on the game-theoretic principle of minimizing Counterfactual Regret. \
The immediate counterfactual regret $r^t(I, a)$ measures the utility gained if player $i$ had played action $a$ at information set $I$ versus following their current strategy $\sigma^t$, weighted by the probability that the opponent played to reach $I$:
$$r^t(I, a) = v_i(\sigma^t|_{I \to a}, I) - v_i(\sigma^t, I)$$

The algorithm updates the strategy for the next iteration $t+1$ using Regret Matching. \
The probability of choosing action $a$ is proportional to its accumulated positive regret $R_+^T(I,a) = \max(0, \sum_{t=1}^T r^t(I, a))$:
$$\sigma^{t+1}(a|I) = \frac{R_+^T(I,a)}{\sum_{a' \in A(I)} R_+^T(I,a')}$$

The fundamental theorem of CFR proves that as the average regret approaches zero, the average strategy profile $\bar{\sigma}^T$ converges to an $\epsilon$-Nash Equilibrium (whereas the current strategy $\sigma^t$ may not). 

Classical tabular CFR requires maintaining a regret table for every possible information set, which is computationally impossible in massive games like No-Limit Texas Hold'em without aggressive domain-specific abstraction. \
**Deep CFR** resolves this by replacing the tabular memory using two distinct deep neural networks:
1. **Advantage Network** (Regret Network): Approximates the cumulative regret vector for all actions at a given information set. It is trained by minimizing the Mean Squared Error (MSE) between the network prediction and the sampled counterfactual values from traversal buffers.
2. **Strategy Network**: Because the Nash Equilibrium is defined by the average strategy over time (not the current regret-matching strategy), this network learns to approximate $\bar{\sigma}$ by training on a reservoir-sampled history of the agent's past probability distributions. 

By bypassing manual abstraction entirely, Deep CFR demonstrated that deep function approximation can be synthesized with rigorous regret minimization to master high-dimensional hidden-information paradigms.

### 13: Evolution Strategies: A Scalable Alternative to Gradient-Based RL
Deep Reinforcement Learning typically relies on the backpropagation algorithm to compute precise gradients through vast neural networks. \
However, backpropagation in RL is fraught with inherent instability due to severely delayed rewards, vanishing gradients over long temporal horizons, and the chaotic variance inherent in value function approximation. 

**Evolution Strategies** (ES) represents a radical paradigm shift, treating policy optimization strictly as a "black-box" optimization problem. \
It entirely abandons the MDP mathematical formulation, value functions, and the backpropagation algorithm itself. 
Developed heavily by researchers at OpenAI, modern ES applies Gaussian noise directly to the parameters (weights) of the neural network $\theta$ rather than injecting noise into the action space. \
The goal is to maximize the expected return of a Gaussian-smoothed objective function $J(\theta)$:
$$J(\theta) = \mathbb{E}_{\epsilon \sim \mathcal{N}(0, I)} [F(\theta + \sigma \epsilon)]$$
where $F(\psi)$ is the stochastic, non-differentiable objective function mapping policy weights $\psi$ to the total episode return, and $\sigma$ is the exploration variance scalar. 

By leveraging a second-order Taylor expansion, it is mathematically proven that an unbiased estimator for the gradient of this smoothed objective can be derived purely from finite differences in the high-dimensional parameter space. \
The update rule for the parameters $\theta$ becomes:
$$\nabla_\theta J(\theta) \approx \frac{1}{N\sigma} \sum_{i=1}^N F(\theta + \sigma \epsilon_i) \epsilon_i$$
This estimator implies a highly parallelizable "guess and check" methodology. 

A population of $N$ worker nodes evaluates perturbed versions of the policy $\theta + \sigma \epsilon_i$ in parallel. \
Because the distributed workers require no backpropagation (only forward passes), the computational overhead collapses. \
Furthermore, ES demands virtually zero communication bandwidth. 

Instead of transmitting millions of dense gradient parameters across a network cluster, worker nodes utilizing shared random seeds can reconstruct the noise vectors $\epsilon_i$ locally.\
Consequently, they only need to broadcast a single scalar value—the final episode reward $F(\theta + \sigma \epsilon_i)$—to the central master node. \
This hyper-parallelizability allowed researchers to distribute training across thousands of CPU cores, scaling linearly and reducing training times for complex Atari games and 3D humanoid walking tasks from days to mere minutes. 
 
This explicitly proves that evolutionary algorithms can rival, and occasionally exceed, advanced gradient-based actor-critic architectures in game-playing domains.

### 14. Intrinsic Motivation: Overcoming Hard-Exploration Bottlenecks
Standard RL algorithms, such as DQN or PPO, rely fundamentally on explicit, dense reward signals. 

In "hard-exploration" game environments—such as the infamous Atari game Montezuma's Revenge or Pitfall!—the extrinsic reward signal $r_{ext}$ is exceedingly sparse, deceptive, or non-existent for long horizons. \
In these states, standard entropy-based random exploration (like $\epsilon$-greedy) fails catastrophically, as the probability of stochastically stumbling upon a complex, precise sequence of actions decays exponentially with the sequence length. \

**Intrinsic Motivation** resolves this by mathematically simulating biological curiosity. \
It augments the sparse external MDP reward with a dense internal reward term $r_{int}$ computed dynamically by the agent: $R_{total} = r_{ext} + \beta r_{int}$.

A seminal, mathematically elegant technique is **Random Network Distillation** (RND). RND operates by initializing a randomly parameterized, permanently frozen Target Network $f_\phi(s)$. \
A parallel **Predictor Network** $\hat{f}_\theta(s)$ is trained to emulate the target network's outputs on visited states via gradient descent. \
The intrinsic reward is formulated precisely as the Mean Squared Error (MSE) of this distillation:
$$r_{int}(s) = \|\hat{f}_\theta(s) - f_\phi(s)\|^2$$

When the agent explores a novel, unseen state, the predictor network (having never trained on this input) yields high error, generating a massive, dense pseudo-reward that propels the agent toward uncharted territory. \
As the state becomes familiar through repeated visits, the predictor $\theta$ converges to the fixed target $\phi$, and the error decays to zero. 

Crucially, because the target $f_\phi$ is deterministic, RND avoids the "Noisy TV" trap that plagues forward dynamics models (which attempt to predict stochastic future states). \
To solve the most severe exploration bottlenecks in the entire Atari-57 suite, the Never Give Up (NGU) algorithm pairs global curiosity (RND) with strict episodic memory. \
NGU maintains a dynamic Episodic Memory Buffer $M$ containing the embedding vectors of states visited in the current episode. \
It calculates a pseudo-count $N(s_t)$ using a kernel density estimate over the $k$-nearest neighbors in memory:
$$r_{episodic}(s_t) \approx \frac{1}{\sqrt{N(s_t) + c}}$$

This strict episodic novelty prevents the agent from entering infinite loops (visiting the same safe state repeatedly within one life). \
Finally, to balance this exploration with the need to collect actual rewards, NGU integrates Universal Value Function Approximators (UVFA). \
The agent learns a family of policies $\pi(a|s, \beta)$ conditioned on the intrinsic reward coefficient $\beta$. 

This allows the agent to simultaneously learn pure exploration policies (high $\beta$) and pure exploitation policies (low $\beta$), enabling it to scour complex environments exhaustively while reliably preserving the capacity to exploit discovered extrinsic rewards.

### 15: Safe Reinforcement Learning and Constrained MDPs
As RL agents transition from digital board games to physical robotics, autonomous driving, and critical infrastructure management, prioritizing raw reward performance is no longer sufficient. \
Catastrophic failures during exploration can result in real-world physical damage or financial ruin. Safe Reinforcement Learning systematically addresses this by explicit behavioral constraint, migrating the mathematical foundation from standard MDPs to **Constrained Markov Decision Processes** (CMDPs). \
A CMDP is defined by the tuple $\langle \mathcal{S}, \mathcal{A}, \mathcal{P}, \mathcal{R}, \mathcal{C}, \gamma, \mathbf{\xi} \rangle$. \
The environment emits a primary reward signal $R(s,a)$ designed to encourage task completion, and a set of auxiliary cost signals $C_k(s,a)$ representing explicit safety violations (e.g., high joint torque, proximity to hazards, or violation of traffic laws). \
The optimization objective is rigorously constrained to maximize the expected return while keeping the expected cumulative cost $J_{C_k}(\pi)$ below specific safety thresholds $\xi_k$:
$$\max_\pi J_R(\pi) \quad \text{subject to} \quad J_{C_k}(\pi) \leq \xi_k, \quad \forall k$$

Directly solving this constrained optimization is computationally intractable for model-free RL. \
The standard mathematical resolution is the Lagrangian Relaxation. \
The constrained problem is converted into an unconstrained saddle-point optimization problem through the introduction of dynamic Lagrange multipliers $\lambda_k \ge 0$:

$$\max_\pi \min_{\lambda \ge 0} \mathcal{L}(\pi, \lambda) = J_R(\pi) - \sum_k \lambda_k (J_{C_k}(\pi) - \xi_k)$$

During the training phase, the system solves this Min-Max game via Primal-Dual Optimization. \
The policy optimizer performs gradient ascent to maximize $\mathcal{L}$ (improving reward), while the dual optimizer performs gradient descent on $\lambda$ (increasing the penalty strength). \
If the agent breaches the threshold ($J_{C_k} > \xi_k$), the gradient term $(J_{C_k} - \xi_k)$ becomes positive, causing $\lambda_k$ to integrate upwards. \
This mathematically overwhelms the reward signal, forcing the policy gradients back into the safe subset of the action space. \
However, Lagrangian methods only guarantee safety asymptotically (upon convergence). 

For strict safety guarantees during training, Constrained Policy Optimization (CPO) is employed. \
CPO approximates the objective and constraints using second-order Taylor expansions within a Trust Region, analytically solving for the optimal policy update step that strictly improves reward without violating the safety boundary. \
Alternatively, Lyapunov-based methods construct barrier functions (Lyapunov candidates) that mathematically guarantee the agent remains within the stable region of the state space, providing absolute worst-case safety assurances.

### 16: Hierarchical Reinforcement Learning: The Options Framework
In complex game domains with vast temporal horizons—such as real-time strategy games or expansive open-world navigation—atomic decisions operating at the frequency of milliseconds or individual frames create exponentially deep search trees and impossible credit assignment problems. \
Humans circumvent this cognitive overload by planning via temporal abstraction, organizing behaviors into high-level, logical sub-tasks. 

**Hierarchical Reinforcement Learning** (HRL) institutionalizes this concept through the **Options Framework** (Sutton, Precup, & Singh, 1999), extending the classical MDP into a Semi-Markov Decision Process (SMDP). 

An Option represents a temporally extended macro-action. \
Mathematically, an option is defined as a rigid tuple $\omega = \langle \mathcal{I}_\omega, \pi_\omega, \beta_\omega \rangle$:
- Initiation Set ($\mathcal{I}_\omega \subseteq \mathcal{S}$): The specific subset of states from which the option can be legally initiated.
- Intra-option Policy ($\pi_\omega: \mathcal{S} \times \mathcal{A} \to [0, 1]$): The low-level control policy that executes atomic actions $a$ while the option is active.
- Termination Condition ($\beta_\omega: \mathcal{S} \to [0, 1]$): The probability that the option terminates upon reaching state $s$, returning control to the higher-level meta-controller.

By abstracting a long sequence of granular actions (e.g., navigating a complex maze to a door) into a single logical step (e.g., the option "Exit Room"), the higher-level meta-controller operates over a dramatically compressed state-action space.

In the SMDP framework, transitions occur over variable, stochastic durations $\tau$. \
Consequently, the transition dynamics and reward functions must be integrated over the time interval the option consumes. \
The SMDP Bellman Equation for the value of a state $s$ under a hierarchical policy $\mu$ is:
$$V^\mu(s) = \sum_{\omega \in \Omega(s)} \mu(\omega|s) \left[ R(s, \omega) + \sum_{s', \tau} \gamma^\tau P(s', \tau | s, \omega) V^\mu(s') \right]$$

Here, the reward $R(s, \omega)$ is the expected discounted cumulative reward received during the execution of option $\omega$:

$$R(s, \omega) = \mathbb{E} \left[ r_{t+1} + \gamma r_{t+2} + \dots + \gamma^{\tau-1} r_{t+\tau} \mid \mathcal{E}(\omega, s_t = s) \right]$$

This hierarchical architecture permits unparalleled domain generalization. \
If a low-level option (e.g., "Build Barracks" in StarCraft) is thoroughly trained in one subset of the environment, it can be instantly leveraged by a meta-controller in an entirely new strategic context without retraining. \
Furthermore, state abstraction allows sub-policies to ignore vast amounts of irrelevant global information, preventing the curse of dimensionality from paralyzing optimization in hyper-scale multi-agent task planning frameworks.

### 17: Model-Based RL in Games: From MuZero to EfficientZero
While model-free agents (like DQN or PPO) require tens of millions of frames to master video games via trial-and-error, Model-Based RL achieves unprecedented sample efficiency by internally learning the dynamics of the environment. \
This enables rigorous forward planning without requiring true, empirical environment interactions. 

The absolute zenith of this approach evolved from AlphaZero into MuZero, and ultimately to EfficientZero, which drastically accelerated sample efficiency on visually complex environments like Atari.

**MuZero** (Schrittwieser et al., 2020) fundamentally discarded the requirement of a perfect, pre-programmed simulator (like the hard-coded rules of Chess). \
Instead, it learns a proprietary Latent World Model defined by three functions:
1. **Representation Function** ($h_\theta$): Maps historical observations to a latent state: $s^0 = h_\theta(o_1, \dots, o_t)$.
2. **Dynamics Function** ($g_\theta$): Predicts the next latent state and immediate reward given an action: $s^{k+1}, r^{k+1} = g_\theta(s^k, a^k)$.
3. **Prediction Function** ($f_\theta$): Estimates the policy and value from a latent state: $\mathbf{p}^k, v^k = f_\theta(s^k)$.

By applying MCTS exclusively within this learned, abstract latent space, MuZero attained superhuman performance across Atari games while remaining entirely blind to the underlying pixel dynamics. \
However, MuZero still demanded immense datasets (hundreds of millions of frames) to properly structure its latent space. 

**EfficientZero** (Ye et al., 2021) revolutionized this paradigm to achieve >100% human-level performance on Atari using only two hours of real-time gameplay experience (the Atari 100k benchmark). \
It implemented three profound mathematical adjustments:
1. **Self-Supervised Consistency Loss**: MuZero's latent space possessed no ground truth; it was shaped solely by end-to-end value gradients.  \
EfficientZero grounds this space by forcing the predicted latent state $\hat{s}_{t+k}$ to align with the actual observation-encoded latent $s_{t+k}$ using a SimSiam architecture. \
The loss minimizes the negative cosine similarity, utilizing a stop-gradient operation to prevent representation collapse:
$$\mathcal{L}_{SSL}(\theta) = -\frac{1}{2} \left( \mathcal{D}(\hat{s}_{t+1}, \text{stop\_grad}(s_{t+1})) + \mathcal{D}(\text{stop\_grad}(\hat{s}_{t+1}), s_{t+1}) \right)$$

2. **End-to-End Value Prefix**: To mitigate error accumulation over long planning horizons, EfficientZero modifies the MCTS backup. \
Instead of relying solely on summing single-step reward predictions (which have high variance), it predicts a Value Prefix—the discounted sum of rewards—allowing the search to look ahead multiple steps with greater stability.

3. **Model-Based Off-Policy Correction** (Reanalyze): To stabilize training on limited data, EfficientZero aggressively utilizes Reanalyze. \
Since data in the replay buffer was generated by an old, inferior policy $\pi_{old}$, its stored value targets are stale. \
EfficientZero re-runs the current MCTS model on these old states to generate fresh, corrected policy and value targets ($\pi_{MCTS}, v_{MCTS}$) for the loss function. \
This ensures that the network always trains against the highest-quality targets available, effectively converting off-policy data into on-policy supervision.

### 18: Advanced Neural Architecture in Board Games: KataGo and AZ-Hive
The generic AlphaZero algorithm, while profoundly powerful in its mathematical simplicity, provides only a broad conceptual template. \
Achieving extreme computational efficiency and tactical mastery requires deeply integrated, domain-specific mathematical configurations. \
The architectural innovations within KataGo and AZ-Hive exemplify the state-of-the-art in applied game-theoretic RL.

**KataGo** (Wu, 2019) drastically outpaced AlphaZero’s training efficiency by augmenting the optimization objective with highly structured Auxiliary Targets. \
In standard AlphaZero, the value head is trained on a single, binary signal ($z \in \{-1, 1\}$) received at the game's conclusion. \
KataGo decomposes this sparse signal by forcing the network to solve simultaneous auxiliary tasks, most notably *Ownership Prediction*. \
The network predicts which player will ultimately control every intersection $l$ on the board. \
The mathematical loss is formulated as a weighted cross-entropy:
$$\mathcal{L}_{own} = -w_o \sum_{l \in \text{board}} \sum_{p \in \{\text{black, white}\}} o(l, p) \log (\hat{o}(l, p))$$

This innovation transforms a single scalar update into a dense, board-wide gradient matrix, punishing the network for fundamental miscalculations in local territory and accelerating spatial representation learning. \
Furthermore, KataGo introduced **Global Pooling layers**. 
While standard convolutions possess a localized receptive field, global pooling aggregates the mean and maximum values of feature channels across the entire $19 \times 19$ grid. 
This feeds holistic spatial context—such as global "ko" threats—directly into the residual blocks, allowing the network to synchronize distant tactical relationships.

Applying AlphaZero principles to the board-less game of Hive, the **AZ-Hive** project reveals the mathematical implications of State Space Encoding. \
Because Hive is played on a continuous surface without fixed borders, standard Cartesian matrix encoding fails. \
AZ-Hive circumvents this using a Hexagonal Axial Coordinate System $(q, r)$ to mathematically map relative tile connectivity into rigid, dense binary planes (tensors). \
AZ-Hive further utilizes **State Abstraction** to manage the complexity of the piece-stacking mechanic. \
By exploring varying representations—such as a **Symmetric Representation** (ignoring tiles trapped underneath Beetles to bound the spatial mapping) or a **Simple Representation** (abstracting piece types to prioritize the win-condition)—the project proves that the numerical parameterization of the environment is as consequential as the underlying optimization algorithm. The exact mathematical configuration of this design space dictates the tensor shapes, the network architecture, and the learning velocity of the agent.

## Section C. References 
1. "The Cost of Reinforcement Learning for Game Engines: The AZ-Hive Case-study", ICPE Proceedings 2022
2. "Reinforcement Learning Enhanced LLMs: A Survey", arXiv:2509.16679v1"
3. "Using reinforcement learning in chess engines", ResearchGate
4. "Mastering the game of Go without human knowledge", arXiv:1712.01815
5. "Mastering the game of go without human knowledge", Nature
6. "Multiagent Reinforcement Learning", Emergent Mind
7. "Distributional Reinforcement Learning", Emergent Mind
8. "Scalable Hierarchical Reinforcement Learning for Hyper Scale Multi-Robot Task Planning", arXiv:2412.19538
9. "Counterfactual Regret Minimization", Steven Gong Notes
10. "Regret Matching and CFR", Imperial College London
11. "Learn AI Game Playing Algorithm Part III", Medium
12. "Predictive Discounted Counterfactual Regret Minimization", arXiv:2404.13891v1
13. "Multi-agent Q-learning with linear value decomposition", OpenReview
14. "QMIX: mixing Q with monotonic factorization", MARLlib
15. "Distributional RL categorical Bellman operator", arXiv:2202.00769v4
16. "Distributional RL with Quantile Regression", MIT Press
17. "Fully Parameterized Quantile Function for Distributional RL", NeurIPS
18. "Distributional RL Workshop", EWRL
19. "Conservative Q-Learning", Google Sites
20. "Conservative Q-Learning for Offline RL", NeurIPS
21. "Implicit Q-Learning", GitHub Pages
22. "Offline Reinforcement Learning", BAIR Blog
23. "EfficientZero: Mastering Atari with Limited Data", YouTube / Paper
24. "EfficientZero: Human ALE Sample Efficiency", Alignment Forum
25. "The Cost of Reinforcement Learning for Game Engines", ICPE 2022
26. "Monte Carlo Tree Search and Alpha-Beta pruning", Charles University
27. "Alpha Zero's move encoding", AI StackExchange
28. "Intrinsic Reinforcement Learning", Emergent Mind
29. "Curiosity-driven learning", PMC
30. "Hierarchical Reinforcement Learning options framework", MDPI
31. "Markov decision process", Wikipedia
32. "Between MDPs and semi-MDPs: A framework for temporal abstraction", Sutton et al.
33. "Hierarchical Reinforcement Learning", arXiv:2508.17751v1
34. "Hierarchical RL", Harsha Kokel Blog
35. "Evolution Strategies as a Scalable Alternative to Reinforcement Learning", OpenAI
36. "Evolution Strategies vs Reinforcement Learning", TU Delft
37. "Never Give Up: Learning Directed Exploration Strategies", arXiv:2002.06038
38. "Never Give Up: Learning Directed Exploration", Semantic Scholar
39. "Reinforcement Learning Course", Charles University
40. "Implicit Q-Learning (IQL)", Emergent Mind
41. "Implicit Q-Learning in Offline RL", Emergent Mind
42. "Offline RL with Implicit Q-Learning", NeurIPS 2021
43. "Safe Reinforcement Learning Lagrangian formulation", IJCAI 2024
44. "A Lyapunov-based Approach to Safe Reinforcement Learning", NeurIPS
45. "Safe Reinforcement Learning", arXiv:2505.17342v1
46. "Safe RL Lagrangian Method", CMU Thesis
47. "QMIX value decomposition network architecture", MDPI
48. "Weighted QMIX", NeurIPS 2020
49. "QMIX: Monotonic Value Function Factorisation", ICML 2018
50. "KataGo technical details auxiliary tasks", arXiv:1902.10565
51. "KataGo general techniques", arXiv:1902.10565
52. "EfficientZero: How it works", LessWrong
53. "EfficientZero SimSiam for temporal consistency", Reddit
54. "Deep Counterfactual Regret Minimization", arXiv:1901.07621
55. "Deep CFR", arXiv:1811.00164
56. "Distributional reinforcement learning categorical Bellman operator", EWRL
57. "Categorical and Quantile Distributional RL", PMLR
58. "Distributional Bellman operator math derivation", arXiv:2202.00769v5
59. "Deriving Bellman's equation in reinforcement learning", StackExchange
60. "QMIX hypernetwork equations and monotonicity proof", MDPI
61. "QMIX value decomposition hypernetwork equations", arXiv:2408.15381v2
62. "Evolution Strategies for games algorithm", OpenAI
63. "Evolution Strategies as a Scalable Alternative to RL", TU Delft
64. "Categorical Bellman Operator projection step", PMLR
65. "Categorical Distributional Reinforcement Learning", arXiv:1707.06887
66. "EfficientZero loss function components", OpenReview
67. "EfficientZero temporal consistency", Reddit
68. "Remaking EfficientZero", Alignment Forum
69. "Deep CFR value network and strategy network loss functions", arXiv:2511.08174v1
70. "Deep Counterfactual Regret Minimization", PMLR
71. "QMIX hypernetwork architecture equations", Emergent Mind
72. "QMIX hypernetworks for parameter generation", arXiv:2502.12605v1
73. "KataGo auxiliary policy target loss", Branton DeMoss
74. "KataGo global pooling", arXiv:1902.10565
75. "NGU episodic reward and RND intrinsic reward", UMontreal Scholaris
76. "Intrinsic Motivation RND", arXiv:2509.03790v2
77. "KataGo auxiliary loss functions mathematical definitions", arXiv:1902.10565
78. "KataGo score ownership", Online-Go Forums
79. "Hive board game state encoding for neural networks", ICPE 2022
80. "C51 algorithm projection step", PMLR
81. "Deep CFR strategy network update rule", PMLR
82. "Single Deep CFR", arXiv:1901.07621
83. "EfficientZero SimSiam consistency loss", TU Delft Repository
84. "Implicit Q-Learning expectile loss function", Emergent Mind
85. "IQL expectile loss function", Lee Yng Do Blog
86. "Evolution strategies gradient estimate formula", Inference.vc
87. "Evolution Strategies optimization", OpenAI
88. "AZ-Hive move encoding hexagonal grid", Charles University