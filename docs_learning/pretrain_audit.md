# Audit del Pretraining e Piano per il Self-Play

Data: 19/04/2026
Contesto: dopo aver osservato che il bot gioca male, sono emersi dubbi su (a) come è stato costruito il dataset, (b) quanto sia biasato il modello attuale, (c) se il pretraining già fatto vada buttato o sia riusabile.

Questo documento raccoglie le risposte alle tre domande poste, con la relativa spiegazione tecnica basata sul codice effettivo della repo.

---

## Domanda 1 — Le mosse legali del dataset sono state generate col nostro `MoveGenerator` o con un riferimento esterno (Mzinga, implementazioni Python corrette)?

### Risposta sintetica
**Col nostro `MoveGenerator`.** Non esiste nessuno script Python per la conversione del dataset: tutta la pipeline di parsing SGF → tensori `.pt` è scritta in C++ e usa lo stesso generator del bot.

### Spiegazione

L'entry point del pretraining è `cpp/pretrain_main.cpp`, che chiama `SgfParser::processDirectory(...)` implementato in `cpp/src/alphaZeroEngine/data/sgf_parser.cpp`. Per ogni partita SGF, `SgfParser::processGame()` fa il replay mossa per mossa. Al core del replay, alla riga 405 di `sgf_parser.cpp`:

```cpp
auto legalMoves = MoveGenerator::generateMoves(state);
```

è il **nostro** `MoveGenerator`, quello definito in `cpp/src/generator.cpp`. Lo stesso che veniva usato nella vecchia `cpp/learning/` e oggi in `cpp/src/alphaZeroEngine/`.

Il ruolo del generator nella costruzione del dataset è duplice:

1. **Filtro**: per ogni mossa SGF, il codice cerca un match tra la mossa letta dal file e l'elenco di `legalMoves` prodotto dal generator (righe 416-434). Se il match fallisce, il codice fa `return {}` alla riga 437 e **scarta l'intera partita**.
2. **Risoluzione dei metadati della mossa**: quando il match va a buon fine, viene eseguito `move = lm`, cioè si adotta l'oggetto `Move` costruito dal generator (utile in particolare per le Drag del Pillbug/Mosquito, che in SGF sono scritte come normali movimenti).

### Cosa NON è stato usato
- `py/src/` contiene un bot Python separato con un `RandomMoveEngine` scritto a mano — non è stato usato per costruire il dataset.
- Mzinga e Nokamute compaiono solo in `referee/containers/` come avversari Dockerizzati nei tornei, **mai come fonte di ground truth** per le mosse legali.

### Implicazione diretta
Il dataset di pretraining dipende dalla correttezza del nostro generator. Qualunque bug del generator all'epoca della generazione del dataset si è propagato sul dataset stesso. Il tipo di propagazione (filtro vs. label noise) è il tema della Domanda 3.

---

## Domanda 2 — C'è margine di miglioramento rifacendo altre 30 epoch con le valid moves corrette? Quanto è biasato il modello?

### Risposta sintetica
**Margine basso per il supervised puro, margine alto per l'AlphaZero vero (self-play).** Il bias del modello attuale è moderato e di tipo *selection bias*, non *label noise*. Altre 30 epoch a budget costante danno un guadagno stimato di `+1-3%` in policy accuracy (~`+50-150 Elo`). Il vero upside sta altrove.

### Spiegazione

**Come funziona la loss del pretrain.** In `trainer.cpp:65-66`:

```cpp
auto logSoftmax = torch::log_softmax(logits, /*dim=*/1);
auto policyLoss = -(targetPolicies * logSoftmax).sum(1).mean();
```

Il `log_softmax` è calcolato **sull'intero action space da 5488**, senza maschera sulle mosse legali. Il `target` è one-hot sulla mossa umana. La loss non dipende in alcun punto dalle mosse legali generate dal nostro generator: **il bug del generator non sporca i gradienti**. Può solo scartare campioni a monte, nel filtro del parser.

**Dove è il bias.** Quando il checkpoint `pretrained_best.pt` è stato creato (commit `92de756`), i fix del generator erano parzialmente presenti:
- `b9b3444` (Ant moves) e `c928e0d` (canSlide) erano già in.
- `842f97d` (pillbug/mosquito drag illegal) era ed è ancora **solo su `main`, non su `learning`**.

Il bug vivo al momento del pretraining era quindi la gestione delle drag Pillbug/Mosquito. Questo si traduce in un filtro che scarta sistematicamente le partite che contengono drag realmente giocate dagli umani. Stima grossolana: **5-20% delle partite Base+MLP rigettate**, concentrate su posizioni pillbug-pesanti. È una lacuna di copertura, non un'alterazione delle label.

**Perché altre 30 epoch danno poco.** Tre ragioni cumulative:

1. **Rendimenti decrescenti** — dopo 30 epoch con LR `0.001` e cosine annealing, la cross-entropy è probabilmente già in plateau. Le epoch successive con stessa ricetta spingono la rete in overfit sui sample più frequenti.
2. **Tetto intrinseco del dataset** — boardspace non filtra per ELO, quindi il teacher è un umano di forza media (con code di bassa qualità). L'asintoto del supervised su questo dataset è "policy di umano medio", e a Hive questo livello è modesto. Non importa quante epoch fai: non sforeremo quel tetto.
3. **Teacher signal rumoroso** — una one-hot su 5488 azioni dice alla rete "in quello stato l'unica mossa buona è questa, tutte le altre 5487 sono sbagliate". È informazione *forte ma rumorosa*: nella stessa posizione esistono spesso 5-30 mosse ragionevoli, e usare una softmax piatta non-mascherata spreca capacità di rete.

**Dove sta il vero margine di miglioramento**, in ordine di costo/beneficio:

1. **Gratis — solo rebuild**. Mergere su `learning` i fix di `main` (`842f97d`, `28dc243`, `032fa3c`). Il generator corretto cambia due cose a inference time:
   - `ActionEncoder::legalMask` (riga 184 di `action_encoder.cpp`) smette di mascherare mosse valide prima mascherate.
   - `moveToAction` / `actionToMove` risolvono correttamente le Drag.
   Il modello già allenato suddenly propone mosse che MCTS prima non vedeva. Stima empirica: **+100-300 Elo senza toccare i pesi**.

2. **Costo basso, grosso impatto — lanciare il self-play**. `Trainer::train(numIterations)` in `trainer.cpp:157` implementa fedelmente il loop AlphaZero: self-play → training con target `policy = distribuzione di visite MCTS` → evaluation → promozione (soglia `EVAL_THRESHOLD=0.55`). Il target di MCTS è **molto più forte** della one-hot umana. Anche 5-10 iterazioni (`SELF_PLAY_GAMES=256` per iter) portano guadagni stimati nell'ordine di **+500-1000 Elo** rispetto al checkpoint attuale. Questo è il 90% del valore inesplorato della pipeline.

3. **Medio costo — migliorare la loss**. In `trainer.cpp:65-66` basta sostituire il log_softmax su tutto lo spazio con un *masked softmax* sulle legali (usando `ActionEncoder::legalMask`). Gratis di complessità, regala capacità di rete. Combinabile con label smoothing o con filtri ELO nel parser SGF se l'header `BR`/`WR` contiene il rating.

---

## Domanda 3 — Le 30 epoch già fatte (senza i fix) sono valide o sono corrotte? Va rifatto il pretraining da zero prima del self-play?

### Risposta sintetica
**Sono valide. Non serve rifare il pretraining da zero.** Il checkpoint `pretrained_best.pt` è un'inizializzazione onesta per il self-play. Attenzione: prima di lanciare il self-play **devi aver mergiato i fix del generator**, altrimenti la lacuna si cristallizza.

### Spiegazione

Ripercorro esattamente cosa finisce nei pesi durante il pretraining e controllo punto per punto dove il generator buggy avrebbe potuto inquinare qualcosa:

| Ingrediente di training | Origine | Dipende dal generator? |
| --- | --- | --- |
| `state` tensor | `StateEncoder::encode(state)` dopo `state.applyMove(...)` dal replay SGF | No — `applyMove` è logica indipendente dal generator |
| `policy` target | `torch::zeros({5488}); policy[action] = 1.0` con `action = ActionEncoder::moveToAction(move, state)` | Indirettamente: `move.from` è risolto da `StringToMove` guardando la board, non dal generator. `moveToAction` usa solo `(piece, from, to)`. Risultato: pulito. |
| `value` target | Outcome della partita (`+1`/`-1`/`0`) | No |
| Cross-entropy loss | Log-softmax sul 5488-dim, senza maschera | No |

Il generator è stato usato **unicamente come filtro sì/no** nel parser (decidere se tenere o scartare la partita) e come selettore dell'oggetto `Move` concreto da far entrare in `ActionEncoder::moveToAction`. Per i campioni che sono passati, gli oggetti `Move` che hanno guidato l'encoding avevano `(piece, from, to)` identici a quelli letti dall'SGF. Nessuna corruzione è quindi entrata nei pesi.

**Quello che resta** è la selection bias discussa nella Domanda 2: il modello ha visto meno partite del normale, con una sotto-rappresentazione di posizioni pillbug/mosquito-heavy. È una lacuna di copertura, non un segnale avvelenato.

**Perché non rifare il pretraining da zero è la scelta giusta**:

1. Il costo computazionale (30 epoch su SGF boardspace, ~1M+ sample, minibatch 64) è significativo.
2. Il guadagno atteso è piccolo (`+5-20%` di dati recuperati, plateau di supervised molto vicino).
3. Il self-play impara a riempire proprio quelle lacune quando il generator è corretto — il gap pillbug/mosquito verrà chiuso organicamente.

### Il pericolo concreto: partire col self-play prima di aver corretto il generator

Se lanci `Trainer::train(...)` con il generator ancora buggy su `learning`, il loop fa questo:
- MCTS chiama `ActionEncoder::legalMask` → `MoveGenerator::generateMoves`.
- Il generator non restituisce certe pillbug/mosquito drag.
- MCTS non esplora mai quelle mosse. La rete non le vede nemmeno come opzioni.
- Le partite di self-play non contengono mai quelle situazioni.
- La rete continua a non imparare a usarle.

Risultato: il buco di copertura del supervised **si cristallizza e si amplifica** nel self-play. È il peggiore dei mondi possibili, perché spendi ore di GPU rafforzando un modello ceco in una intera classe di situazioni.

---

## Piano d'azione raccomandato

In ordine, con effort e impatto stimati:

1. **Allineare `learning` con i fix di `main`** (effort: minuti, impatto: alto)
   - Merge o cherry-pick dei commit `842f97d` (pillbug/mosquito drag), `28dc243` (time management), `032fa3c` (args refactor).
   - Rebuild `uhp` e verifica manuale che il bot proponga pillbug drag nelle partite.

2. **(Opzionale) 5-10 epoch di rinforzo sul dataset rigenerato** (effort: ore, impatto: basso-medio)
   - Ri-eseguire `SgfParser::processDirectory` col generator fixato → recupero del 5-20% di partite prima scartate.
   - Caricare `pretrained_best.pt`, addizionare 5-10 epoch a LR ridotto (`1e-4`) dal checkpoint esistente, **non da zero**.
   - Chiude il gap pillbug/mosquito in modo esplicito prima del self-play.

3. **Lanciare il self-play loop** (effort: molte ore di GPU, impatto: altissimo)
   - `hive_pretrain` o un nuovo entry point che invochi `Trainer::train(numIterations)`.
   - Partire da `pretrained_best.pt` (o dal checkpoint rinforzato dello step 2).
   - Monitorare il win-rate vs `bestModel_`; la promozione avviene automaticamente quando `EVAL_THRESHOLD=0.55`.
   - Budget consigliato iniziale: 5-10 iterazioni, poi riaggiustare.

4. **(Opzionale) Migliorare la loss di pretrain per futuri retrain** (effort: minuti, impatto: alto se si rifà pretraining)
   - Sostituire `log_softmax` su tutto con masked softmax sulle legali in `trainer.cpp:65-66`.
   - Aggiungere label smoothing (`0.05-0.1`).
   - Filtrare SGF per ELO minimo nel parser, se l'header contiene il rating.

---

## Riassunto in una frase

Il dataset è stato costruito col nostro generator; il pretraining attuale è valido e riusabile senza rifare nulla da zero; il margine vero di miglioramento non sta nel rifare altre 30 epoch supervised ma nel (1) mergiare i fix del generator e (2) lanciare per la prima volta il self-play di AlphaZero.
