# Neet Patel
# CNT 5505
# Project 1
# 01/29/19

if __name__ == '__main__':
    h_dict = {0b101: 0b1000000, 0b111: 0b0100000, 0b110: 0b0010000, 0b011: 0b0001000, 0b100: 0b0000100, 0b010: 0b0000010, 0b001: 0b0000001}
    code_word_dict = {k[:4]: int(k[4:], 2) for k in [''.join([str(i) for i in [a, b, c, d, a ^ b ^ c, b ^ c ^ d, a ^ b ^ d]]) for a in range(2) for b in range(2) for c in range(2) for d in range(2)]}
    with open('proj1_testdata') as f:
        print(''.join([chr(int(tup[0] + tup[1], 2)) for tup in zip(*[iter([bin(int(code_word, 2) ^ h_dict[int(code_word[4:], 2) ^ code_word_dict[code_word[:4]]])[2:].zfill(7)[:4] for code_word in [''.join([d for d in line.split()]) for line in f]])] * 2)]))
