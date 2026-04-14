# Guida al Training — High-Hive AlphaZero

## Panoramica

Il sistema di training ha due modalità:

1. **Pre-training supervisionato** — la rete impara a imitare giocatori umani da partite SGF (boardspace.net)
2. **Self-play AlphaZero** — la rete gioca contro se stessa e migliora iterativamente

L'idea è fare prima il pre-training per dare alla rete un punto di partenza decente, poi passare al self-play per superare il livello umano.

---

## Prerequisiti

### Sistema operativo

Testato su macOS e Linux. Windows è supportato da LibTorch ma potrebbe richiedere aggiustamenti al CMake.

### Strumenti di build

| Strumento | Versione minima | Come installare |
|-----------|----------------|-----------------|
| C++ compiler | C++20 (GCC 10+, Clang 12+, MSVC 2019+) | Già presente su macOS (Xcode CLT) e la maggior parte delle distro Linux |
| CMake | 3.20+ | `brew install cmake` (macOS) / `sudo apt install cmake` (Ubuntu) |
| Make o Ninja | qualsiasi | Già presente su macOS e Linux |

Verificare le versioni:

```bash
c++ --version    # deve supportare C++20
cmake --version  # deve essere >= 3.20
```

Su macOS installare Xcode Command Line Tools se non presenti:

```bash
xcode-select --install
```

Su Ubuntu/Debian installare i build tools:

```bash
sudo apt update
sudo apt install build-essential cmake
```

### LibTorch (richiesto solo per il modulo learning)

LibTorch è la distribuzione C++ di PyTorch. **Non serve Python** — è una libreria C++ standalone.

#### Opzione A — Download pre-compilato (consigliato)

1. Andare su https://pytorch.org/get-started/locally/
2. Selezionare: **LibTorch** → **C++/Java** → il proprio OS → **CUDA 12.x** (se hai GPU NVIDIA) o **CPU**
3. Scaricare lo zip e estrarlo

```bash
# Esempio macOS/Linux con CPU
wget https://download.pytorch.org/libtorch/cpu/libtorch-macos-arm64-2.6.0.zip
unzip libtorch-macos-arm64-2.6.0.zip -d ~/

# Esempio Linux con CUDA 12.4
wget https://download.pytorch.org/libtorch/cu124/libtorch-cxx11-abi-shared-with-deps-2.6.0%2Bcu124.zip
unzip libtorch-cxx11-abi-shared-with-deps-2.6.0+cu124.zip -d ~/
```

Il path da usare dopo sarà `~/libtorch` (o dove hai estratto).

> **Nota versione**: gli URL cambiano con le nuove release di PyTorch. Controllare sempre la pagina ufficiale per i link aggiornati.

#### Opzione B — Via conda (se usi già conda)

```bash
conda install pytorch cpuonly -c pytorch    # solo CPU
conda install pytorch pytorch-cuda=12.4 -c pytorch -c nvidia  # con CUDA
```

Il path di LibTorch sarà dentro l'environment conda:
```bash
LIBTORCH_PATH=$(python -c "import torch; print(torch.utils.cmake_prefix_path)")
```

#### Opzione C — Build da sorgente (avanzato)

Solo se serve una configurazione custom. Seguire https://github.com/pytorch/pytorch#from-source

### CUDA (opzionale, per training su GPU)

Il training su GPU è **molto più veloce** (10-50x rispetto a CPU). Necessario per il self-play in tempi ragionevoli.

1. Verificare di avere una GPU NVIDIA compatibile:
   ```bash
   nvidia-smi
   ```

2. Installare CUDA Toolkit dalla pagina NVIDIA:
   https://developer.nvidia.com/cuda-downloads

3. Installare cuDNN:
   https://developer.nvidia.com/cudnn

4. Scaricare la versione di LibTorch corrispondente alla versione CUDA installata.

> **Su macOS con Apple Silicon (M1/M2/M3/M4)**: CUDA non è disponibile. LibTorch usa comunque l'accelerazione MPS (Metal Performance Shaders) automaticamente dalla versione 2.0+. Scaricare la versione CPU di LibTorch per macOS ARM64.

### Dati SGF per il pre-training (opzionale)

Per il pre-training supervisionato servono partite umane in formato SGF da boardspace.net:

1. Andare su https://www.boardspace.net/english/about_hive.html
2. Cercare la sezione "Game Archives" o "SGF Downloads"
3. Scaricare l'archivio delle partite (tipicamente un .zip con migliaia di file .sgf)
4. Estrarre in una cartella, es. `~/hive-games/`

---

## Compilazione

### Solo game engine (senza LibTorch)

```bash
cd cpp
mkdir -p build && cd build
cmake ..
make
```

Produce solo l'eseguibile `high_hive` (engine UHP).

### Con modulo learning (richiede LibTorch)

```bash
cd cpp
mkdir -p build && cd build
cmake .. -DENABLE_LEARNING=ON -DCMAKE_PREFIX_PATH=~/libtorch
make
```

Sostituire `~/libtorch` con il path effettivo dove hai estratto LibTorch.

Se la compilazione fallisce con errori su torch, verificare:
```bash
# Il path è corretto?
ls ~/libtorch/share/cmake/Torch/TorchConfig.cmake

# Se il file non esiste, il path è sbagliato
```

Questo produce tre eseguibili:
- `high_hive` — engine UHP (non richiede LibTorch a runtime)
- `hive_pretrain` — pre-training supervisionato
- `hive_train` — self-play AlphaZero

### Generare compile_commands.json (per l'IDE)

Per eliminare i falsi errori in VS Code / CLion:

```bash
cd cpp/build
cmake .. -DCMAKE_EXPORT_COMPILE_COMMANDS=ON
ln -sf build/compile_commands.json ../compile_commands.json
```

---

## 1. Pre-training supervisionato

### Comando

```bash
./hive_pretrain --sgf-dir /path/to/sgf/games --epochs 30 --checkpoint-dir checkpoints/
```

### Opzioni CLI

| Flag | Default | Descrizione |
|------|---------|-------------|
| `--sgf-dir` | (obbligatorio) | Cartella con i file `.sgf` |
| `--epochs` | 30 | Numero di epoche di training |
| `--checkpoint-dir` | `checkpoints/` | Dove salvare i modelli |

### Cosa fa

1. **Caricamento SGF**: `SgfParser::processDirectory()` legge tutti i file `.sgf` dalla cartella
2. **Ricostruzione partite**: per ogni partita, ricostruisce lo stato turno per turno con `GameState::apply()`
3. **Validazione mosse**: prima di applicare ogni mossa, verifica che sia nelle `legalMoves()` correnti. Se una mossa è illegale, scarta l'intera partita (lo stato dopo sarebbe corrotto)
4. **Generazione sample**: per ogni posizione valida crea un training sample:
   - `state` — tensore `[24, 26, 26]` che codifica la board
   - `policy` — one-hot sulla mossa effettivamente giocata
   - `value` — outcome della partita (+1 bianco vince, -1 nero vince, 0 patta)
5. **Training**: allena la rete su questi sample per N epoche
   - Loss = cross-entropy (policy) + MSE (value)
   - SGD con momentum 0.9, weight decay 1e-4, LR 0.001
6. **Salvataggio**: checkpoint in `checkpoints/pretrained.pt`

### Risultato atteso

- ~60-80% delle partite SGF supera la validazione (il resto viene scartato per mosse illegali, varianti vecchie, bug del client)
- Dopo 1 epoca la rete dovrebbe battere il random
- Dopo 30 epoche dovrebbe giocare a livello umano medio

---

## 2. Self-play AlphaZero

### Comando

```bash
./hive_train --iterations 50 --checkpoint-dir checkpoints/
```

Per riprendere da un checkpoint precedente (es. dopo il pre-training):

```bash
./hive_train --iterations 50 --resume pretrained
```

### Opzioni CLI

| Flag | Default | Descrizione |
|------|---------|-------------|
| `--iterations` | 50 | Numero di iterazioni di training |
| `--checkpoint-dir` | `checkpoints/` | Dove salvare i modelli |
| `--resume` | (nessuno) | Nome del checkpoint da cui riprendere |

### Cosa fa — ciclo di ogni iterazione

#### Step 1 — Self-play (256 partite)

La rete gioca contro se stessa. Per ogni mossa:

- **MCTS** esegue 800 simulazioni dalla posizione corrente
- Ogni simulazione segue il ciclo: **Select → Expand → Backprop**
  - **Select**: scende nell'albero seguendo la formula PUCT (`Q + C * P * sqrt(N_parent) / (1+N)`)
  - **Expand**: raggiunta una foglia, la rete neurale valuta la posizione producendo una policy (distribuzione sulle mosse) e un value (chi sta vincendo)
  - **Backprop**: il value viene propagato all'indietro nell'albero, negato ad ogni livello (perché i giocatori si alternano)
- Le visite MCTS normalizzate diventano la **policy target** (la distribuzione "corretta" secondo la ricerca)
- **Dirichlet noise** aggiunto ai prior del nodo root per garantire esplorazione
- **Temperatura**: 1.0 per le prime 15 mosse (esplora diverse linee), 0.1 dopo (gioca la mossa migliore)
- **Safety cutoff**: max 200 mosse per partita
- A fine partita, ogni posizione riceve l'outcome finale come **value target**

#### Step 2 — Training (1000 step di gradiente)

- Campiona batch da 512 posizioni dal **replay buffer** (buffer circolare da 500K posizioni)
- Forward pass sulla rete → logits policy + value
- **Loss** = cross-entropy(policy_MCTS, logits) + MSE(outcome, value_predetto)
- **Ottimizzatore**: SGD con momentum 0.9, weight decay 1e-4
- **Learning rate**: cosine annealing (parte da 0.01 e scende gradualmente)

#### Step 3 — Valutazione (400 partite)

- Il modello appena allenato gioca 400 partite contro il "best model" corrente
- I colori si alternano (200 da bianco, 200 da nero) per evitare bias
- Nessun noise, temperatura 0 (gioco deterministico)
- Si calcola il win rate del nuovo modello

#### Step 4 — Promozione

- Se il nuovo modello vince **≥ 55%** → diventa il nuovo "best model" e i suoi pesi vengono copiati
- Altrimenti si torna ai pesi del best model e si ricomincia
- Checkpoint salvato in ogni caso (`best_iter_N.pt` o `latest_iter_N.pt`)

---

## Flusso dati

```
GameState ──→ StateEncoder ──→ Tensor [24, 26, 26]
                                      │
                                      ▼
                                   HiveNet
                                  (19 ResBlocks)
                                   │       │
                            Policy ▼       ▼ Value
                         [5488 logits]   [-1, +1]
                               │
                     ┌─────────┴──────────┐
                     ▼                    ▼
              ActionEncoder          MCTS usa il value
           (applica mask legali,     per backpropagation
            softmax → probabilità)    nell'albero
                     │
                     ▼
              MCTS visit counts
                     │
            ┌────────┴────────┐
            ▼                 ▼
     Selezione mossa    Policy target
     (da giocare)       (per training)
```

### Codifica dello stato (24 canali)

| Canali | Contenuto |
|--------|-----------|
| 0-7 | Pezzi del giocatore corrente per tipo (Q, B, S, G, A, L, M, P) |
| 8-15 | Pezzi dell'avversario per tipo |
| 16 | Altezza della pila (normalizzata /6) |
| 17 | Colore del pezzo in cima (+1 mio, -1 avversario, 0 vuoto) |
| 18 | Celle dove è legale piazzare |
| 19 | Livello di accerchiamento della mia regina (0-1) |
| 20 | Livello di accerchiamento della regina avversaria |
| 21 | Punti di articolazione (pezzi bloccati dalla One Hive Rule) |
| 22 | Indicatore turno (tutto 1, sempre dal punto di vista del corrente) |
| 23 | Pezzi rimanenti in mano (frazione uniforme) |

La board esagonale (coordinate assiali) viene mappata su una griglia 26x26, centrata sul centroide dell'alveare corrente.

### Codifica delle azioni (5488)

```
azione = direzione × pezzo_sorgente × pezzo_riferimento
       = 7 × 28 × 28
       = 5488
```

- **7 direzioni**: 6 direzioni esagonali + 1 "sopra" (beetle/piazzamento)
- **28 pezzi**: 14 per colore (Q, B1, B2, S1, S2, G1-G3, A1-A3, L, M, P)

---

## Architettura della rete (HiveNet)

```
Input [B, 24, 26, 26]
  │
  ▼
Conv3x3 (24 → 256) + BatchNorm + ReLU
  │
  ▼
ResidualBlock × 19
  (Conv3x3 → BN → ReLU → Conv3x3 → BN → +skip → ReLU)
  │
  ├──────────────────────────────┐
  ▼                              ▼
Policy Head                    Value Head
  Conv1x1 (256→2)               Conv1x1 (256→1)
  BN + ReLU                     BN + ReLU
  Flatten (2×26×26 = 1352)      Flatten (1×26×26 = 676)
  FC (1352 → 5488)              FC (676 → 256) + ReLU
  │                              FC (256 → 1) + Tanh
  ▼                              ▼
Logits [B, 5488]               Value [B, 1] ∈ [-1, +1]
```

~15-25M parametri totali.

---

## Iperparametri principali

| Parametro | Valore | Dove |
|-----------|--------|------|
| MCTS simulazioni | 800 | `config.h` |
| C_PUCT | 2.5 | `config.h` |
| Dirichlet alpha | 0.15 | `config.h` |
| Dirichlet epsilon | 0.25 | `config.h` |
| Temperatura alta (prime 15 mosse) | 1.0 | `config.h` |
| Temperatura bassa (dopo) | 0.1 | `config.h` |
| Batch size | 512 | `config.h` |
| Replay buffer | 500K | `config.h` |
| Learning rate | 0.01 | `config.h` |
| Momentum | 0.9 | `config.h` |
| Weight decay | 1e-4 | `config.h` |
| Blocchi residui | 19 | `config.h` |
| Filtri | 256 | `config.h` |
| Partite self-play/iterazione | 256 | `config.h` |
| Partite valutazione | 400 | `config.h` |
| Soglia promozione | 55% | `config.h` |
| Max mosse per partita | 200 | `config.h` |
| Epoche pre-training | 30 | `config.h` |

Tutti gli iperparametri sono in `cpp/learning/config/headers/config.h` e possono essere modificati prima della compilazione.

---

## Workflow consigliato

```
1. Scarica partite SGF da boardspace.net

2. Pre-training supervisionato (~ore su GPU)
   ./hive_pretrain --sgf-dir games/ --epochs 30

3. Self-play AlphaZero (~giorni/settimane su GPU)
   ./hive_train --iterations 50 --resume pretrained

4. Il modello migliore è in checkpoints/best_iter_N.pt
```

### Stima tempi e costi

- **Pre-training**: ~2-3 giorni su 1 GPU (RTX 4090), ~25-50 EUR
- **Self-play**: 50-80 iterazioni in ~2-5 settimane su RTX 4090, ~500 EUR
- Ogni iterazione: 256 partite self-play + 1000 step training + 400 partite eval

---

## Struttura file del modulo learning

```
cpp/learning/
├── config/headers/config.h          # Tutti gli iperparametri
├── nn/
│   ├── headers/
│   │   ├── neural_net.h             # HiveNet (ResNet CNN)
│   │   ├── state_encoder.h          # GameState → Tensor
│   │   └── action_encoder.h         # Move ↔ indice azione
│   ├── neural_net.cpp
│   ├── state_encoder.cpp
│   └── action_encoder.cpp
├── mcts/
│   ├── headers/mcts.h               # MCTSNode + MCTS search
│   └── mcts.cpp
├── training/
│   ├── headers/
│   │   ├── replay_buffer.h          # Buffer circolare 500K
│   │   ├── self_play.h              # Generazione partite
│   │   └── trainer.h                # Training loop + eval
│   ├── replay_buffer.cpp
│   ├── self_play.cpp
│   └── trainer.cpp
├── data/
│   ├── headers/sgf_parser.h         # Parser SGF boardspace.net
│   └── sgf_parser.cpp
└── alphazero_engine.h               # Engine per giocare con il modello
```

---

## Usare il modello allenato

Una volta ottenuto un checkpoint, `AlphaZeroEngine` (in `learning/alphazero_engine.h`) implementa l'interfaccia `Engine::getBestMove()`. Carica il modello dal file `.pt`, esegue MCTS per scegliere la mossa, e comunica via protocollo UHP.

Per integrarlo nell'engine UHP, basta sostituire `RandomEngine` con `AlphaZeroEngine` in `uhp.h`:

```cpp
// In uhp.h, cambiare:
std::unique_ptr<Engine> engine = std::make_unique<RandomEngine>();
// Con:
std::unique_ptr<Engine> engine = std::make_unique<AlphaZeroEngine>("checkpoints/best.pt");
```

Questo richiede che l'eseguibile sia compilato con `ENABLE_LEARNING=ON`.
