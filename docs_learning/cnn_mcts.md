# CNN + MCTS in stile AlphaZero per Hive

## Indice

1. [Perché questo approccio](#1-perché-questo-approccio)
2. [I 3 componenti del sistema](#2-i-3-componenti-del-sistema)
3. [Componente 1: La Rete Neurale (CNN)](#3-componente-1-la-rete-neurale-cnn)
4. [Componente 2: MCTS](#4-componente-2-mcts-monte-carlo-tree-search)
5. [Componente 3: Self-Play](#5-componente-3-self-play-il-ciclo-di-apprendimento)
6. [Warm Start Supervisionato](#6-warm-start-supervisionato)
7. [Perché non altre architetture](#7-perché-non-altre-architetture)
8. [Stack tecnologico e vincoli](#8-stack-tecnologico-e-vincoli)
9. [Pipeline completa](#9-pipeline-completa)
10. [Budget e risorse](#10-budget-e-risorse)

---

## 1. Perché questo approccio

### Il problema fondamentale di Hive

Hive ha un **fattore di ramificazione medio di ~60** (quante mosse legali ci sono in media
per turno). Per confronto, gli scacchi ne hanno ~35 e il Go ~250. Questo significa che se
vuoi guardare 4 mosse avanti, devi valutare:

```
60^4 = ~13.000.000 di posizioni
```

Anche con potatura alpha-beta perfetta (che riduce il branching a ~sqrt(60) ≈ 7.7),
restano ~7.7^4 ≈ 3.500 posizioni.

Ma il vero collo di bottiglia non è la ricerca: è la **funzione di valutazione**.
Nei motori tradizionali (alpha-beta), la valutazione di una posizione è scritta a mano
da un programmatore. Per gli scacchi, funziona bene: puoi contare il materiale (pedoni,
torri, regine hanno un valore noto), valutare la struttura pedonale, il controllo del centro,
e ottenere una valutazione ragionevole.

Per Hive, questo approccio fallisce:

- **Non ci sono catture**, quindi non c'è materiale da contare.
- **Tutti i pezzi restano in gioco**, quindi il valore di un pezzo è interamente
  nella sua posizione e libertà di movimento.
- **L'euristica migliore conosciuta** è il conteggio di accerchiamento della Regina
  (`pezzi intorno alla Regina nemica - pezzi intorno alla mia Regina`), che è utile
  ma approssimativo e non fornisce alcun gradiente nella maggior parte delle posizioni
  dove entrambe le Regine sono minimamente minacciate.
- **Le relazioni posizionali** (blocchi, sbarramenti, dominanza dello Scarabeo,
  flessibilità di posizionamento) richiedono un riconoscimento sofisticato di pattern
  che è estremamente difficile da codificare manualmente.

Nessun programmatore umano è mai riuscito a scrivere una funzione di valutazione
davvero buona per Hive.

### Come AlphaZero risolve entrambi i problemi

AlphaZero (DeepMind, 2017) ha raggiunto prestazioni sovrumane negli Scacchi, nel Go e
nello Shogi — tutti giochi a informazione perfetta, deterministici e a somma zero,
esattamente come Hive. L'approccio risolve entrambi i problemi:

1. **Impara da solo a valutare le posizioni**: una rete neurale sostituisce la funzione
   di valutazione artigianale. La rete vede milioni di posizioni e impara quali pattern
   portano alla vittoria.

2. **Impara da solo quali mosse sono promettenti**: la rete produce anche una
   distribuzione di probabilità sulle mosse (policy), che riduce il branching factor
   effettivo da 60 a ~5-10. MCTS concentra le visite sulle mosse indicate dalla policy.

Il progetto AZ-Hive (ICPE 2022) ha dimostrato che questo approccio funziona
specificamente per Hive, sebbene a costo computazionale elevato.

---

## 2. I 3 componenti del sistema

Il sistema è composto da esattamente **3 pezzi** che lavorano insieme:

```
                    ┌─────────────────────┐
                    |   RETE NEURALE (CNN) |
                    |                     |
   stato del ──────>|  - Policy head ────────> "queste 5 mosse sono promettenti"
   gioco            |  - Value head  ────────> "il bianco sta vincendo (+0.3)"
                    └─────────────────────┘
                              |
                              v
                    ┌─────────────────────┐
                    |       MCTS          |
                    |  usa policy + value |──────> mossa scelta
                    |  per cercare meglio |
                    └─────────────────────┘
                              |
                              v
                    ┌─────────────────────┐
                    |     SELF-PLAY       |
                    |  gioca contro se    |──────> dati di training
                    |  stesso             |
                    └─────────────────────┘
```

- La **rete neurale** (CNN) valuta posizioni e suggerisce mosse.
- **MCTS** usa la rete per esplorare l'albero di gioco in modo intelligente.
- Il **self-play** genera partite di allenamento: la rete gioca contro se stessa,
  e i risultati vengono usati per migliorare la rete.

Il ciclo si auto-rinforza: la rete migliora → MCTS cerca meglio → produce dati
migliori → la rete migliora ancora.

---

## 3. Componente 1: La Rete Neurale (CNN)

### Cosa fa

La rete prende lo stato corrente del gioco e produce **due output simultanei**:

1. **Policy**: una distribuzione di probabilità su tutte le mosse possibili.
   "Quanto è promettente ciascuna mossa?"
2. **Value**: un singolo numero tra -1 e +1.
   "Chi sta vincendo in questa posizione?"

### Input: codifica dello stato di gioco

Il tabellone di Hive viene codificato come un'**immagine multi-canale** di dimensione
**C x 26 x 26**, dove C è il numero di canali (feature planes).

La griglia esagonale viene mappata su una griglia rettangolare usando **coordinate
offset (odd-q)**. Una griglia 26x26 è abbondantemente sufficiente per qualsiasi
partita reale di Hive (il numero massimo di pezzi è 28, e l'alveare non si estende
mai oltre ~13 celle in ogni direzione).

Ogni canale è una griglia 26x26 che rappresenta un'informazione diversa:

```
Canali 1-8:   Pezzi del giocatore corrente per tipo
              (Q, B, G, S, A, M, L, P) — maschere binarie 0/1.
              Es: canale 1 ha un 1 dove c'è la mia Regina, 0 altrove.

Canale 9-16:  Pezzi dell'avversario per tipo — stessa struttura.

Canale 17:    Altezza della pila in ogni cella (0-4, normalizzata a [0,1]).
              Rilevante per lo stacking degli Scarabei.

Canale 18:    Colore del pezzo in cima alla pila.
              +1 = giocatore corrente, -1 = avversario, 0 = cella vuota.

Canale 19:    Celle di posizionamento legali per il giocatore corrente (0/1).

Canale 20:    Conteggio dei pezzi adiacenti alla mia Regina (normalizzato).

Canale 21:    Conteggio dei pezzi adiacenti alla Regina avversaria (normalizzato).

Canale 22:    Pezzi bloccati — punti di articolazione nel grafo dell'alveare (0/1).

Canale 23:    Numero del turno (valore costante su tutta la griglia, normalizzato).

Canale 24:    Pezzi rimanenti in mano (codificati come conteggi diffusi).
```

**Totale: 24 canali x 26 x 26 = 16.224 valori di input.**

> **Nota sulla griglia quadrata vs esagonale**: un filtro convolutivo 3x3 standard
> cattura 9 celle, ma un esagono ha solo 6 vicini. 2 dei 9 elementi del filtro
> corrispondono a non-vicini. Nella pratica, la rete **impara a ignorare** quelle
> posizioni (assegna pesi ~0 ai 2 elementi irrilevanti). AZ-Hive ha dimostrato
> che questo non è un problema significativo.

### Output 1: Policy Head

La policy head produce un vettore di **5.488 valori** che rappresentano la probabilità
di ogni possibile mossa.

La codifica dello spazio delle azioni segue l'approccio di OpenSpiel per Hive:

```
5.488 = 7 direzioni x 28 pezzi sorgente x 28 pezzi riferimento
```

Dove:
- **7 direzioni**: le 6 direzioni esagonali + "sopra" (per mosse di Scarabeo/Coccinella)
- **28 pezzi sorgente**: tutti i pezzi del gioco (14 bianchi + 14 neri).
  Per i piazzamenti, si usa una codifica speciale per il tipo di pezzo dalla mano.
- **28 pezzi riferimento**: il pezzo di destinazione usato come riferimento posizionale.

Le **mosse illegali vengono mascherate a probabilità zero** prima della selezione
MCTS. Questo avviene applicando una maschera binaria ai logit della policy
(impostando a -infinito i logit delle mosse illegali) prima del softmax.

### Output 2: Value Head

La value head produce un **singolo scalare in [-1, +1]** tramite una funzione `tanh`:

- **+1**: il giocatore corrente vince sicuramente
- **-1**: il giocatore corrente perde sicuramente
- **0**: posizione pari / incerta
- **valori intermedi**: gradi di vantaggio/svantaggio

### Architettura interna

```
Input: 24 x 26 x 26
    |
[Conv 3x3, 256 filtri, stride 1, padding 1]
[BatchNorm]
[ReLU]
    |
[Blocco Residuo x 19]
    Ogni blocco:
    |── Conv 3x3, 256 filtri, padding 1
    |── BatchNorm
    |── ReLU
    |── Conv 3x3, 256 filtri, padding 1
    |── BatchNorm
    |── + skip connection (somma l'input del blocco)
    └── ReLU
    |
    |
    +───────────────────────+
    |                       |
    v                       v
 Policy Head            Value Head
    |                       |
 [Conv 1x1, 2 filtri]   [Conv 1x1, 1 filtro]
 [BatchNorm + ReLU]      [BatchNorm + ReLU]
 [Flatten]               [Flatten]
 [FC -> 5488]            [FC -> 256]
 [Softmax mascherato]    [ReLU]
                         [FC -> 1]
                         [Tanh]
```

**Parametri totali: ~15-25M** (a seconda della larghezza dei blocchi residui).

I **blocchi residui** (ResNet) sono fondamentali: le skip connection permettono alla
rete di essere profonda (19 blocchi = 38 layer convolutivi + il primo = 39 totali)
senza soffrire del problema del vanishing gradient. AlphaZero originale usa
esattamente questa struttura.

### Perché una CNN e non un MLP semplice

Le CNN sfruttano due proprietà:

1. **Invarianza traslazionale**: un pattern locale (es. "Scarabeo sopra la Regina")
   viene riconosciuto indipendentemente da dove si trova sulla griglia. Questo
   riduce enormemente il numero di parametri e migliora la generalizzazione.

2. **Località**: i filtri 3x3 catturano relazioni tra celle adiacenti. I blocchi
   residui impilati estendono il campo recettivo: con 19 blocchi, la rete "vede"
   pattern che si estendono per ~19 celle in ogni direzione — più che sufficiente
   per l'intera partita di Hive.

---

## 4. Componente 2: MCTS (Monte Carlo Tree Search)

### Differenza rispetto ad Alpha-Beta

**Alpha-Beta**: esplora **tutto** fino a una certa profondità, poi valuta con
un'euristica. Efficiente quando la valutazione è buona e il branching è basso.

**MCTS**: esplora **in modo selettivo**, concentrando il calcolo sulle mosse più
promettenti. Non ha un limite di profondità fisso — può cercare molto in profondità
nelle linee critiche e ignorare le linee inutili.

Con la rete neurale che guida la ricerca, MCTS diventa estremamente potente:
la policy head dice "guarda queste 5 mosse" (su 60 possibili), e la value head
dice "questa posizione vale +0.3" senza bisogno di cercare oltre. Il risultato
è una ricerca profonda ed efficiente.

### Come funziona: il ciclo MCTS

Per scegliere una mossa, MCTS esegue il seguente ciclo **800 volte** (800 simulazioni):

#### Passo 1: Selezione

Parti dalla **radice** (la posizione corrente del gioco). Ad ogni nodo, scegli il
figlio con il punteggio **PUCT** (Predictor Upper Confidence bound for Trees) più alto:

```
PUCT(nodo) = Q(nodo) + c_puct * P(nodo) * sqrt(N(padre)) / (1 + N(nodo))
```

Dove:
- **Q(nodo)** = valore medio delle visite precedenti di questo nodo (exploitation).
  "Quanto è stato buono questo nodo finora?"
- **P(nodo)** = prior dalla policy head della rete neurale.
  "La rete pensa che questa mossa sia buona?"
- **N(nodo)** = numero di visite a questo nodo.
  Un nodo poco visitato ha un bonus di esplorazione alto.
- **N(padre)** = visite totali del genitore.
- **c_puct = 2.5** = costante che bilancia esplorazione e sfruttamento.

La formula bilancia due forze:
- **Exploitation** (Q): preferisci mosse che hanno dimostrato di funzionare.
- **Exploration** (il termine con P e sqrt): prova mosse poco esplorate,
  specialmente quelle che la rete considera promettenti.

All'inizio (poche visite), il termine di esplorazione domina → MCTS esplora.
Man mano che le visite crescono, il termine Q domina → MCTS sfrutta.

Continua a selezionare figli fino a raggiungere un **nodo foglia** (mai visitato).

#### Passo 2: Espansione e Valutazione

Quando arrivi a un nodo foglia (una posizione mai valutata):

1. Passa lo stato di gioco alla rete neurale.
2. Ottieni: **policy** (prior P per ogni mossa legale) e **value** (v).
3. Crea un nodo figlio per ogni mossa legale, assegnandogli il prior dalla policy.

Questo è il punto chiave: **la rete neurale sostituisce sia il rollout casuale**
(il value sostituisce il "gioca fino alla fine") **sia l'euristica artigianale**
(il value è appreso, non scritto a mano).

#### Passo 3: Backpropagation

Propaga il valore **all'indietro** dalla foglia alla radice, aggiornando ogni nodo
lungo il percorso:

```
Per ogni nodo dal foglia alla radice:
    N(nodo) += 1                  // incrementa le visite
    Q_totale(nodo) += valore      // accumula il valore
    valore = -valore              // nega ad ogni livello!
```

La **negazione** è fondamentale: il vantaggio di un giocatore è lo svantaggio
dell'avversario. Se una posizione è buona per il Bianco (+0.7), è cattiva per
il Nero (-0.7). Alternando il segno ad ogni livello, MCTS modella correttamente
il gioco a due giocatori.

#### Passo 4: Selezione della mossa

Dopo 800 simulazioni, scegli la mossa con il **maggior numero di visite** (non il
valore Q più alto — le visite sono una stima più robusta). Opzionalmente, applica
una temperatura:

```
probabilita(mossa) = N(mossa)^(1/temperatura) / somma(N(tutte_mosse)^(1/temperatura))
```

- **temperatura = 1.0** (prime 15 mosse): esplorazione, scelte diverse → dati di
  training più vari.
- **temperatura → 0** (mosse successive): sfruttamento, scegli la mossa migliore
  deterministicamente.

### Rumore di Dirichlet

Alla radice, prima di iniziare le 800 simulazioni, si aggiunge **rumore di Dirichlet**
ai prior della policy:

```
P'(mossa) = (1 - epsilon) * P(mossa) + epsilon * Dir(alpha)
```

Con epsilon = 0.25 e alpha = 0.15 per Hive.

Questo garantisce che MCTS esplori anche mosse che la rete non considera promettenti,
evitando che il sistema si "fissi" su un set ristretto di mosse. Senza questo rumore,
la rete potrebbe non scoprire mai strategie innovative durante il self-play.

### Esempio concreto

Posizione: il Bianco deve muovere, ci sono 45 mosse legali.

```
La rete dice:
  - Mossa A (Formica su Regina nera): P = 0.35   ← la rete la ritiene migliore
  - Mossa B (Scarabeo avanza):         P = 0.20
  - Mossa C (piazza Cavalletta):       P = 0.10
  - ... altre 42 mosse con P bassi (0.01 ciascuna)

Dopo 800 simulazioni MCTS:
  - Mossa A: 380 visite, Q = +0.42    ← MCTS conferma
  - Mossa B: 210 visite, Q = +0.38
  - Mossa C: 95 visite,  Q = +0.25
  - ... altre mosse: 1-10 visite ciascuna

Scelta: Mossa A (380 visite = la piu visitata).
```

MCTS ha concentrato l'85% delle simulazioni sulle prime 3 mosse (suggerite dalla
policy), ignorando le 42 mosse meno promettenti. Questo è equivalente a cercare
con un branching factor di ~3 invece di 45.

---

## 5. Componente 3: Self-Play (il ciclo di apprendimento)

### Il concetto

Il sistema impara giocando contro se stesso. Non servono avversari esterni,
non serve conoscenza umana pre-programmata (anche se la usiamo per accelerare,
vedi sezione 6). Il ciclo è:

```
Rete casuale → gioca male → impara dai risultati → gioca meglio → ripeti
```

### Il ciclo nel dettaglio

Ogni **iterazione** del self-play funziona così:

#### Passo 1: Genera partite (Self-Play)

La rete corrente (il "modello migliore attuale") gioca **256 partite** contro se stessa:

- Per ogni posizione nella partita, MCTS esegue 800 simulazioni.
- Registra per ogni posizione:
  - **Stato del tabellone** (input per la rete)
  - **Distribuzione delle visite MCTS** (questo diventa il target per la policy —
    è una versione "migliorata" della policy della rete, perché MCTS aggiunge ricerca)
  - **Giocatore corrente** (per assegnare il valore alla fine)
- Alla fine della partita, registra l'**esito**: +1 (bianco vince), -1 (nero vince), 0 (pareggio).
- L'esito viene assegnato retroattivamente a ogni posizione: +1 se il giocatore
  corrente in quella posizione ha vinto, -1 se ha perso.

#### Passo 2: Addestra la rete

Campiona batch di 512 posizioni dal **replay buffer** (un buffer circolare che
contiene le ultime ~500.000 posizioni dalle partite recenti).

Per ogni posizione, la rete deve:
1. **Predire la policy MCTS** (distribuzione delle visite) — loss di cross-entropy
2. **Predire l'esito della partita** — loss MSE (Mean Squared Error)

```
Loss totale = L_policy + L_value + L_regularization

L_policy = -sum( pi_mcts * log(pi_rete) )
           La rete deve avvicinarsi alla distribuzione delle visite MCTS.

L_value  = (z - v)^2
           z = esito reale della partita (+1/-1/0)
           v = predizione della rete
           La rete deve predire chi vince.

L_reg    = c * ||theta||^2      (c = 1e-4)
           Regolarizzazione L2 per evitare overfitting.
```

Ottimizzatore: **SGD con momento** (momento = 0.9, weight decay = 1e-4).
Learning rate: **cosine annealing** da 0.01 a 0.0001.

#### Passo 3: Valuta il nuovo modello

Il modello appena addestrato gioca **400 partite** contro il modello migliore attuale
(200 come Bianco, 200 come Nero, per eliminare il vantaggio del primo giocatore).

- Se il nuovo modello vince **>= 55%** delle partite, diventa il nuovo "modello migliore".
- Altrimenti, viene scartato e si continua con il modello precedente.

Questa soglia del 55% previene regressioni: il modello deve dimostrare di essere
significativamente migliore prima di essere promosso.

#### Passo 4: Ripeti

Si torna al Passo 1 con il nuovo modello migliore. Ogni iterazione produce
dati di qualità leggermente superiore, che a loro volta producono un modello
leggermente migliore.

### Perché il ciclo funziona: il Policy Improvement Theorem

La magia sta in un fatto matematico: la **policy MCTS è sempre almeno buona quanto
la policy della rete da sola**. MCTS aggiunge ricerca alla policy della rete,
quindi il risultato è strettamente migliore.

Di conseguenza, quando la rete impara a imitare la policy MCTS, sta imparando
da un "insegnante" che è sempre meglio di lei. E quando la rete migliora,
MCTS (che usa la rete migliorata) diventa ancora meglio. Questo crea un ciclo
virtuoso di miglioramento continuo:

```
Rete v1 → MCTS con v1 (meglio di v1) → target per v2
Rete v2 → MCTS con v2 (meglio di v2) → target per v3
...
```

### Numeri concreti

| Parametro | Valore |
|-----------|--------|
| Simulazioni MCTS per mossa (training) | 800 |
| Simulazioni MCTS per mossa (valutazione) | 400 |
| Partite per iterazione | 256 |
| Dimensione replay buffer | 500.000 posizioni |
| Batch size | 512 |
| Passi di training per iterazione | 1.000 |
| Partite di valutazione | 400 |
| Soglia di vittoria per promozione | 55% |
| Temperatura (prime 15 mosse) | 1.0 |
| Temperatura (dopo 15 mosse) | 0.1 |
| Rumore Dirichlet alpha | 0.15 |
| Rumore Dirichlet epsilon | 0.25 |
| c_puct (costante di esplorazione MCTS) | 2.5 |

---

## 6. Warm Start Supervisionato

### Il problema del cold start

Se si parte da una rete con pesi casuali, le prime migliaia di partite di self-play
sono puro rumore: mosse casuali, nessun segnale utile. Ci possono volere
**settimane di self-play** prima che emerga un gioco sensato.

### La soluzione: pre-training su partite umane

Prima di iniziare il self-play, pre-addestriamo la rete su **partite di giocatori
umani reali**. La rete impara a:
- **Imitare le mosse umane** (policy supervisionata)
- **Predire chi vince** (value supervisionato)

### Dataset: boardspace.net

Il sito boardspace.net ospita un archivio di partite di Hive dal 2006 al 2026:

- ~20 anni di archivi, 60-90 zip per anno, ~100-200KB ciascuno
- Formato: SGF (Smart Game Format) con notazione Hive specifica
- Stima conservativa: **50.000-100.000+ partite totali**
- URL: `http://www.boardspace.net/hive/hivegames/`

Le partite includono giocatori di tutti i livelli, dai bot ai giocatori esperti.
Anche partite di livello medio sono utili: insegnano alla rete pattern di base
come "non lasciare la Regina esposta" e "le Formiche sono mobili".

### Pipeline del warm start

```
1. Scarica gli archivi (tutti gli anni, ~200MB compressi)
2. Decomprimi e parsa i file SGF → lista di mosse per partita
3. Per ogni partita, ricostruisci ogni posizione intermedia
4. Per ogni posizione, registra:
   - Stato del tabellone (input della rete)
   - Mossa giocata dall'umano (target policy — one-hot sulla mossa giocata)
   - Esito della partita (target value — +1/-1/0)
5. Addestra la rete con apprendimento supervisionato:
   - Loss policy: cross-entropy tra output della rete e mossa umana
   - Loss value: MSE tra output della rete e esito reale
6. Dopo 20-50 epoche, la rete gioca a livello umano-base
```

### Beneficio stimato

Il warm start risparmia **5-10x** il tempo di self-play rispetto a partire da zero.
Dopo pochi giorni di training supervisionato su 1 GPU, la rete:
- Batte il giocatore casuale >99%
- Gioca a livello di giocatore umano medio
- Fornisce una base solida per il self-play

Il self-play successivo **supera** il livello umano perché non è limitato
dalla qualità delle mosse umane: esplora strategie che gli umani non considerano.

---

## 7. Perché non altre architetture

### Architetture considerate e scartate

| Architettura | Motivo per cui è stata scartata |
|---|---|
| **Alpha-Beta classico** | La funzione di valutazione artigianale è il collo di bottiglia. Nessuno ha mai scritto una buona euristica per Hive. Potenziale limitato. |
| **MCTS puro (senza rete)** | Senza rete neurale, i rollout casuali sono rumore puro in Hive. Partite troppo lunghe (30-60+ mosse) e il gioco casuale non dà segnale. La ricerca mostra che "riesce a malapena a battere un agente casuale". |
| **GNN + AlphaZero** | Teoricamente superiore: Hive è intrinsecamente un grafo e la GNN lo modella nativamente. Ma non esiste un equivalente C++ di PyTorch Geometric. Implementare GAT da zero in LibTorch aggiunge settimane di lavoro e rischio, mentre LibTorch ha `Conv2d` pronto all'uso. |
| **HexCNN + AlphaZero** | Usa convoluzioni esagonali che modellano correttamente i 6 vicini. Miglioramento marginale ma reale (~5-10%). Scartata perché richiede kernel custom non ottimizzati in LibTorch, e il guadagno non giustifica la complessità. |
| **MuZero** | Estende AlphaZero con un modello dinamico appreso che sostituisce il simulatore. Ma Hive ha regole note e deterministiche: il nostro game engine C++ è un simulatore perfetto. MuZero aggiunge complessità (3 reti invece di 1) senza beneficio. |
| **Ibrido conoscenza umana** | Combina alpha-beta con rete neurale di valutazione. Training rapido ma potenziale limitato: la forza è vincolata dalla qualità dei dati umani e dall'euristica di ricerca. Non raggiunge il livello di AlphaZero con self-play illimitato. |

### Perché CNN specificamente (e non GNN)

La scelta della CNN rispetto alla GNN è **pragmatica**, non teorica:

**GNN è teoricamente superiore perché:**
- Hive è intrinsecamente un grafo (pezzi = nodi, adiacenza = archi)
- Non spreca calcolo sulle celle vuote (una griglia 26x26 è vuota al ~90%)
- Modello più piccolo (~3M parametri vs ~15-25M)
- Migliore generalizzazione, meno overfitting

**CNN è praticamente superiore nel nostro contesto perché:**
- LibTorch ha `Conv2d`, `BatchNorm2d`, blocchi residui **pronti all'uso**
- Non esiste un equivalente C++ di PyTorch Geometric
- Implementare GATv2Conv a mano in C++ = settimane di lavoro aggiuntivo
- AZ-Hive ha dimostrato che la CNN **funziona** per Hive
- Il "waste" sulle celle vuote è compensato dall'ottimizzazione CUDA dei kernel Conv2d

---

## 8. Stack tecnologico e vincoli

### Linguaggio e librerie

| Componente | Tecnologia | Motivazione |
|---|---|---|
| Game engine | **C++20** | Massima velocità per generazione mosse. Codice esistente nel progetto. |
| Rete neurale | **LibTorch (C++)** | API C++ di PyTorch. Supporta Conv2d, BatchNorm, tensori GPU. |
| MCTS | **C++** | Prestazioni critiche: 800 simulazioni per mossa devono essere veloci. |
| Training loop | **C++/LibTorch** | Coerenza con il resto del sistema. SGD, loss computation, gradient update. |
| Parser SGF | **C++** | Per il warm start: parsing delle partite da boardspace.net. |
| Build system | **CMake** | Già in uso nel progetto. |

### Vincoli hardware

| Risorsa | Disponibilità |
|---|---|
| Budget cloud | 500 EUR |
| GPU cloud stimata | 1x RTX 4090 (~0.5-0.8 EUR/ora) → 625-1000 ore |
| GPU cloud alternativa | 1x A100 40GB (~1.5-2 EUR/ora) → 250-330 ore |

### Stime di tempo computazionale

| Fase | Tempo stimato | Costo stimato |
|---|---|---|
| Training supervisionato (warm start) | 2-3 giorni | ~25-50 EUR |
| Self-play (per iterazione, 256 partite) | ~8-16 ore | ~5-13 EUR |
| 100 iterazioni di self-play | ~5-10 settimane | ~400-800 EUR |

Con 500 EUR, sono fattibili circa **50-80 iterazioni di self-play** dopo il warm start.
Questo dovrebbe essere sufficiente per raggiungere un livello **intermedio-avanzato**.

---

## 9. Pipeline completa

### Visione d'insieme

```
FASE A: Preparazione (1-2 settimane di sviluppo)
  ├── Completare il game engine C++ (generatePlacements, generateMovements)
  ├── Implementare CNN in LibTorch
  ├── Implementare MCTS in C++
  └── Scrivere parser SGF per boardspace.net

FASE B: Warm Start Supervisionato (2-3 giorni di GPU)
  ├── Scarica archivi boardspace.net
  ├── Parsa SGF → dataset di posizioni
  ├── Pre-addestra CNN su partite umane
  └── Risultato: rete che gioca a livello umano-base

FASE C: Self-Play Loop (4-8 settimane di GPU)
  ├── Genera partite di self-play (256/iterazione)
  ├── Addestra su replay buffer
  ├── Valuta contro modello migliore (400 partite, soglia 55%)
  ├── Ripeti
  └── Risultato: rete che supera il livello umano

PRODOTTO FINALE:
  Input: stato di gioco
    → CNN inference (~2-5ms su GPU, ~20-50ms su CPU)
    → MCTS 200-1600 simulazioni (~1-4 sec/mossa su GPU)
  Output: mossa migliore
```

### Flusso dei dati

```
Training supervisionato:
  Partite SGF → Parser → (stato, mossa_umana, esito)
                              |
                              v
                    Training supervisionato (20-50 epoche)
                              |
                              v
                    Modello CNN con warm start
                              |
                              v
Self-play:
  Stato di gioco → CNN → (policy, value)
                              |
                              v
                         MCTS (800 sim.)
                              |
                              v
                     Mossa selezionata → Stato successivo
                              |
                     (ripeti fino a fine partita)
                              |
                              v
                     Registro partita: [(stato, policy_MCTS, esito)]
                              |
                              v
                     Replay buffer (500K posizioni, circolare)
                              |
                              v
                     Campiona batch (512) → Calcola loss → Aggiorna pesi
                              |
                              v
                     Nuovo modello → Valuta vs migliore (400 partite)
                              |
                        [vince >= 55%?]
                         /          \
                       Si            No
                       |              |
                   Promuovi      Scarta, continua
                   a "migliore"  con il vecchio
```

---

## 10. Budget e risorse

### Stima dettagliata dei costi cloud

Usando un provider come Vast.ai o Lambda Labs con RTX 4090 (~0.50-0.80 EUR/ora):

| Attivita | Ore GPU | Costo stimato |
|---|---|---|
| Training supervisionato (warm start) | 48-72 | 25-60 EUR |
| Self-play: 50 iterazioni | 400-800 | 200-640 EUR |
| Valutazioni e test | 20-40 | 10-32 EUR |
| **Totale stimato** | **468-912** | **235-732 EUR** |

Con ottimizzazioni (precision mista FP16, batching efficiente, meno simulazioni
nelle prime iterazioni), il costo può essere contenuto entro i 500 EUR.

### Strategia di ottimizzazione del budget

1. **Training progressivo**: inizia con 200 simulazioni MCTS per mossa (invece di 800)
   nelle prime iterazioni di self-play. Aumenta gradualmente.
2. **Precision mista (FP16)**: raddoppia il throughput sulla GPU con perdita minima
   di qualità.
3. **Batch inference**: raggruppa più valutazioni foglia di MCTS in un singolo
   forward pass della rete, sfruttando meglio il parallelismo GPU.
4. **Riutilizzo dell'albero**: tra una mossa e la successiva, riusa il sotto-albero
   MCTS della mossa scelta invece di ripartire da zero.
5. **Spot instances**: usa istanze spot/preemptible per risparmiare 50-70% sui costi
   (con checkpoint frequenti per recuperare le interruzioni).
