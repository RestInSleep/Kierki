import random
import itertools
import sys  
# Generate a random number from 1 to 7
random_number = random.randint(1, 7)

# Generate a random character from the set {N, E, S, W}
random_char = random.choice(['N', 'E', 'S', 'W'])

values = ['2','3','4','5','6','7','8','9', '10', 'J', 'Q', 'K', 'A'] 
colors = ['S', 'H', 'D', 'C']

cards = list(itertools.product(values, colors))

cards = list(map(lambda x: ''.join(x), cards))
random.shuffle(cards)

if len(sys.argv) != 4:
    print('Usage: python generate.py <file_name> <write_option>(a or w) <number_of_rounds>')
    sys.exit()

file_name = sys.argv[1]
write_options = sys.argv[2]
number_of_rounds = int(sys.argv[3])

if write_options != 'w' and write_options != 'a':
    print('Invalid write option')
    sys.exit()

hand_size = 13
# Open the file in write mode
with open(file_name, write_options + '+') as file:
    # Write the random number and character to the file
    for j in range(number_of_rounds):
        random_number = random.randint(1, 7)
        random_char = random.choice(['N', 'E', 'S', 'W'])
        file.write(f"{random_number}{random_char}\n")
        cards = list(itertools.product(values, colors))
        cards = list(map(lambda x: ''.join(x), cards))
        random.shuffle(cards)
        for i in range(0, 52, hand_size):
            file.write(''.join(cards[i:i+hand_size]) + '\n')