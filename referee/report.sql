.print Total play time
SELECT
    timediff(datetime(sum(elapsed_s), 'unixepoch'), datetime(0, 'unixepoch')) as total_time
FROM games;

CREATE VIEW IF NOT EXISTS white_scores AS
SELECT
    timestamp,
    white AS player,
    CASE
        WHEN outcome = 'WhiteWins' THEN 3
        WHEN outcome = 'BlackWins' THEN 0
        WHEN outcome = 'Draw' THEN 1
    END AS score
FROM games;

CREATE VIEW IF NOT EXISTS black_scores AS
SELECT
    timestamp,
    black AS player,
    CASE
        WHEN outcome = 'WhiteWins' THEN 0
        WHEN outcome = 'BlackWins' THEN 3
        WHEN outcome = 'Draw' THEN 1
    END AS score
FROM games;


.print
.print ====================== Leaderboard ===========================
SELECT player, sum(score) AS total_score
FROM (
    SELECT * FROM white_scores
    UNION ALL
    SELECT * FROM black_scores)
GROUP BY player
ORDER BY total_score DESC;

.print
.print ====================== Matches detail =========================
SELECT white, black, outcome FROM games;

CREATE VIEW IF NOT EXISTS game_stats AS
SELECT
    white, black, outcome, elapsed_s,
    length(game_string) - length(replace(game_string, ';', '')) - 2 AS number_of_plies
FROM games;


.print
.print How many times are outcomes occurring?
SELECT outcome, count(*) FROM games GROUP BY outcome;

.print
.print Game length stats
SELECT avg(number_of_plies) as avg_plies, max(number_of_plies) as max_plies, min(number_of_plies) as min_plies
FROM game_stats;

.print Shortest game
SELECT white, black, outcome, number_of_plies
FROM game_stats
GROUP BY ''
HAVING number_of_plies = min(number_of_plies);

.print Longest game
SELECT white, black, outcome, number_of_plies
FROM game_stats
GROUP BY ''
HAVING number_of_plies = max(number_of_plies);

.print
.print ======================= Errors ==========================
select white, black, outcome, outcome_reason, game_string
from games
where outcome_reason != 'normal ending';
