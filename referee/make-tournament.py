from itertools import product

players = [
    "antelligence",
    "beebot",
    # "beelieve",
    "bee-search",
    "haive",
    "hexa_logic",
    "hivemind",
    "neurohive",
    "strhive-to-fly"
]

for white, black in product(players, players):
    if white != black:
        print(f"{white},{black}")

