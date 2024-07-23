#include "skywall.h"

#include <array>
#include <bit>
#include <cmath>
#include <iostream>
#include <sstream>

using namespace std;

using namespace Skywall;

enum{
	King,
	Pawn,
	Knight,
	Bishop,
	Rook,
	Queen
};

uint64_t passedPawnMasks[2][64] = { 
	{217020518514230016, 506381209866536704, 1012762419733073408, 2025524839466146816, 4051049678932293632, 8102099357864587264, 16204198715729174528, 13889313184910721024, 217020518514229248, 506381209866534912, 1012762419733069824, 2025524839466139648, 4051049678932279296, 8102099357864558592, 16204198715729117184, 13889313184910671872, 217020518514032640, 506381209866076160, 1012762419732152320, 2025524839464304640, 4051049678928609280, 8102099357857218560, 16204198715714437120, 13889313184898088960, 217020518463700992, 506381209748635648, 1012762419497271296, 2025524838994542592, 4051049677989085184, 8102099355978170368, 16204198711956340736, 13889313181676863488, 217020505578799104, 506381179683864576, 1012762359367729152, 2025524718735458304, 4051049437470916608, 8102098874941833216, 16204197749883666432, 13889312357043142656, 217017207043915776, 506373483102470144, 1012746966204940288, 2025493932409880576, 4050987864819761152, 8101975729639522304, 16203951459279044608, 13889101250810609664, 216172782113783808, 504403158265495552, 1008806316530991104, 2017612633061982208, 4035225266123964416, 8070450532247928832, 16140901064495857664, 13835058055282163712, 0, 0, 0, 0, 0, 0, 0, 0},
	{0, 0, 0, 0, 0, 0, 0, 0, 3, 7, 14, 28, 56, 112, 224, 192, 771, 1799, 3598, 7196, 14392, 28784, 57568, 49344, 197379, 460551, 921102, 1842204, 3684408, 7368816, 14737632, 12632256, 50529027, 117901063, 235802126, 471604252, 943208504, 1886417008, 3772834016, 3233857728, 12935430915, 30182672135, 60365344270, 120730688540, 241461377080, 482922754160, 965845508320, 827867578560, 3311470314243, 7726764066567, 15453528133134, 30907056266268, 61814112532536, 123628225065072, 247256450130144, 211934100111552, 847736400446211, 1978051601041159, 3956103202082318, 7912206404164636, 15824412808329272, 31648825616658544, 63297651233317088, 54255129628557504}
};
uint64_t isolatedPawnMasks[64] = {144680345676153346, 361700864190383365, 723401728380766730, 1446803456761533460, 2893606913523066920, 5787213827046133840, 11574427654092267680, 4629771061636907072, 144680345676153346, 361700864190383365, 723401728380766730, 1446803456761533460, 2893606913523066920, 5787213827046133840, 11574427654092267680, 4629771061636907072, 144680345676153346, 361700864190383365, 723401728380766730, 1446803456761533460, 2893606913523066920, 5787213827046133840, 11574427654092267680, 4629771061636907072, 144680345676153346, 361700864190383365, 723401728380766730, 1446803456761533460, 2893606913523066920, 5787213827046133840, 11574427654092267680, 4629771061636907072, 144680345676153346, 361700864190383365, 723401728380766730, 1446803456761533460, 2893606913523066920, 5787213827046133840, 11574427654092267680, 4629771061636907072, 144680345676153346, 361700864190383365, 723401728380766730, 1446803456761533460, 2893606913523066920, 5787213827046133840, 11574427654092267680, 4629771061636907072, 144680345676153346, 361700864190383365, 723401728380766730, 1446803456761533460, 2893606913523066920, 5787213827046133840, 11574427654092267680, 4629771061636907072, 144680345676153346, 361700864190383365, 723401728380766730, 1446803456761533460, 2893606913523066920, 5787213827046133840, 11574427654092267680, 4629771061636907072};

struct BoardStateInformation {
	int enPassantSquare;
	bool castlingRights[4];
	int fiftyMoveCount;
};

struct BoardState {
	uint64_t occupiedBoard[3] = {0ull, 0xFFFFull, 0xFFFF000000000000ull};
	uint64_t pieceBoards[8];
	int rawBoard[64];

	int kingLocations[3];

	BoardStateInformation currentState;

	int currentPlayer;
};

static void setPiece(BoardState& board, int row, int col, int piece) {
	int square = 8 * row + col;
	board.rawBoard[square] = piece;
	board.occupiedBoard[piece / 8] |= (1ull) << square;
	board.pieceBoards[piece % 8] |= (1ull) << square;
}

static void removePiece(BoardState& board, int row, int col) {
	int square = 8 * row + col;
	board.occupiedBoard[1] &= ~(1ull << square);
	board.occupiedBoard[2] &= ~(1ull << square);
	board.rawBoard[square] = 0;

	for(int i = 1; i < 7; i++)
		board.pieceBoards[i] &= ~((1ull) << square);
}

static void set_fen(BoardState& board, string fen) {
	bool castlingRights[4] = { 0, 0, 0, 0 };
	int enPassantSquare = 64;
	int fiftyMoveCount = 0;

	int row = 7;
	int col = 0;

	size_t fenIndex = 0;
	string current = fen.substr(0, fen.find(" "));
	string remainder = fen.substr(fen.find(" "));

	while (fenIndex < current.length()) {
		char currentItem = current[fenIndex];

		if (currentItem > 65) {	// ASCII value of A aka piece letters
			switch (currentItem)
			{
				case 'k':
					removePiece(board, row, col);
					setPiece(board, row, col, 17);
					board.kingLocations[2] = 8 * row + col;
					break;
				case 'p':
					removePiece(board, row, col);
					setPiece(board, row, col, 18);
					break;
				case 'n':
					removePiece(board, row, col);
					setPiece(board, row, col, 19);
					break;
				case 'b':
					removePiece(board, row, col);
					setPiece(board, row, col, 20);
					break;
				case 'r':
					removePiece(board, row, col);
					setPiece(board, row, col, 21);
					break;
				case 'q':
					removePiece(board, row, col);
					setPiece(board, row, col, 22);
					break;
				case 'K':
					removePiece(board, row, col);
					setPiece(board, row, col, 9);
					board.kingLocations[1] = 8 * row + col;
					break;
				case 'P':
					removePiece(board, row, col);
					setPiece(board, row, col, 10);
					break;
				case 'N':
					removePiece(board, row, col);
					setPiece(board, row, col, 11);
					break;
				case 'B':
					removePiece(board, row, col);
					setPiece(board, row, col, 12);
					break;
				case 'R':
					removePiece(board, row, col);
					setPiece(board, row, col, 13);
					break;
				case 'Q':
					removePiece(board, row, col);
					setPiece(board, row, col, 14);
					break;
				default:
					printf("ERROR, FEN IMPORT.\n");
					break;
				}
			col++;
		} 
		else if (currentItem ==  47){
			row--;
			col = 0;
		} else {
			int number = (int)(currentItem - 48);
			for (int k = 0; k < number; k++) {
				removePiece(board, row, col);
				col++;
			}
		}
		fenIndex++;
	}

	remainder = remainder.substr(1);
	current = remainder.substr(0, remainder.find(" "));
	remainder = remainder.substr(remainder.find(" "));
	board.currentPlayer = (current[0] == 'w') ? 1 : 2;

	fenIndex = 0;	// Castling
	remainder = remainder.substr(1);
	current = remainder.substr(0, remainder.find(" "));
	remainder = remainder.substr(remainder.find(" "));
	while (fenIndex < current.length()) {
		if (current[fenIndex] == 'K')
			castlingRights[0] = true;
		else if (current[fenIndex] == 'Q')
			castlingRights[1] = true;
		else if (current[fenIndex] == 'k')
			castlingRights[2] = true;
		else if (current[fenIndex] == 'q')
			castlingRights[3] = true;
		fenIndex++;
	}

	// en passant
	remainder = remainder.substr(1);
	current = remainder.substr(0, remainder.find(" "));
	remainder = remainder.substr(remainder.find(" "));
	if (current[0] != '-') {
		int col = (int)(current[0] - 97);
		int row = (int)(current[1] - 49);
		enPassantSquare = row * 8 + col;
	}

	remainder = remainder.substr(1);
	current = remainder.substr(0, remainder.find(" "));
	remainder = remainder.substr(remainder.find(" "));
	fiftyMoveCount = stoi(current);	
	//plyCount = stoi(remainder) * 2 + currentPlayer % 2;

	board.currentState.enPassantSquare = enPassantSquare;
	board.currentState.castlingRights[0] = castlingRights[0];
	board.currentState.castlingRights[1] = castlingRights[1];
	board.currentState.castlingRights[2] = castlingRights[2];
	board.currentState.castlingRights[3] = castlingRights[3];
	board.currentState.fiftyMoveCount = fiftyMoveCount;
}

int popLSB(uint64_t& bitboard) {
    int lsb = std::countr_zero(bitboard);
    bitboard &= bitboard - 1;
    return lsb;
}

static int32_t round_value(tune_t value)
{
	return static_cast<int32_t>(round(value));
}

[[nodiscard]] static auto lsb(const uint64_t bb) {
    return countr_zero(bb);
}

[[nodiscard]] static auto count(const uint64_t bb) {
    return popcount(bb);
}

[[nodiscard]] static auto east(const uint64_t bb) {
    return (bb << 1) & ~0x0101010101010101ULL;
}

[[nodiscard]] static auto west(const uint64_t bb) {
    return (bb >> 1) & ~0x8080808080808080ULL;
}

[[nodiscard]] static uint64_t north(const uint64_t bb) {
    return bb << 8;
}

[[nodiscard]] static uint64_t south(const uint64_t bb) {
    return bb >> 8;
}

[[nodiscard]] static uint64_t nw(const uint64_t bb) {
    return north(west(bb));
}

[[nodiscard]] static uint64_t ne(const uint64_t bb) {
    return north(east(bb));
}

[[nodiscard]] static uint64_t sw(const uint64_t bb) {
    return south(west(bb));
}

[[nodiscard]] static uint64_t se(const uint64_t bb) {
    return south(east(bb));
}

template <typename F>
[[nodiscard]] auto ray(const int sq, const uint64_t blockers, F f) {
    uint64_t mask = f(1ULL << sq);
    mask |= f(mask & ~blockers);
    mask |= f(mask & ~blockers);
    mask |= f(mask & ~blockers);
    mask |= f(mask & ~blockers);
    mask |= f(mask & ~blockers);
    mask |= f(mask & ~blockers);
    return mask;
}

[[nodiscard]] uint64_t knight(const int sq, const uint64_t) {
    const uint64_t bb = 1ULL << sq;
    return (((bb << 15) | (bb >> 17)) & 0x7F7F7F7F7F7F7F7FULL) | (((bb << 17) | (bb >> 15)) & 0xFEFEFEFEFEFEFEFEULL) |
        (((bb << 10) | (bb >> 6)) & 0xFCFCFCFCFCFCFCFCULL) | (((bb << 6) | (bb >> 10)) & 0x3F3F3F3F3F3F3F3FULL);
}

[[nodiscard]] auto bishop(const int sq, const uint64_t blockers) {
    return ray(sq, blockers, nw) | ray(sq, blockers, ne) | ray(sq, blockers, sw) | ray(sq, blockers, se);
}

[[nodiscard]] auto rook(const int sq, const uint64_t blockers) {
    return ray(sq, blockers, north) | ray(sq, blockers, east) | ray(sq, blockers, south) | ray(sq, blockers, west);
}

[[nodiscard]] uint64_t king(const int sq, const uint64_t) {
    const uint64_t bb = 1ULL << sq;
    return (bb << 8) | (bb >> 8) | (((bb >> 1) | (bb >> 9) | (bb << 7)) & 0x7F7F7F7F7F7F7F7FULL) |
        (((bb << 1) | (bb << 9) | (bb >> 7)) & 0xFEFEFEFEFEFEFEFEULL);
}


struct Trace
{
	int score;	
	double endgame_scale;

	int pst[4][6][64][2]{};
	int passedPawns[6][2]{};
	//int isolatedPawns[8][2]{};

	//int threats[4][5][2]{};	
	//int openFiles[4][2]{};
	int bishopPair[2];	
	int material[6][2]{};
};

const int gamephaseInc[6] = {0, 0, 1, 1, 2, 4};
const int kingBuckets[64] = { 0, 0, 0, 0, 1, 1, 1, 1, 0, 0, 0, 0, 1, 1, 1, 1, 0, 0, 0, 0, 1, 1, 1, 1, 0, 0, 0, 0, 1, 1, 1, 1, 0, 0, 0, 0, 1, 1, 1, 1, 0, 0, 0, 0, 1, 1, 1, 1, 0, 0, 0, 0, 1, 1, 1, 1, 0, 0, 0, 0, 1, 1, 1, 1 };

const int psts[][6][64] = {
        {
                {S(5, -47), S(36, -76), S(-9, -57), S(-97, -50), S(3, -50), S(-81, -18), S(41, -51), S(25, -75), S(-14, -34), S(-15, -31), S(-33, -22), S(-68, -19), S(-63, 8), S(-50, 4), S(13, -18), S(12, -29), S(-91, -12), S(-37, -8), S(-48, 0), S(-68, 4), S(-86, 32), S(-83, 24), S(-51, 9), S(-70, -3), S(-157, -9), S(-49, 5), S(-67, 18), S(-98, 28), S(-90, 45), S(-86, 40), S(-93, 31), S(-89, 8), S(-73, -5), S(-6, 12), S(12, 20), S(-6, 31), S(-31, 46), S(-5, 42), S(-60, 53), S(-71, 27), S(-143, 31), S(43, 18), S(117, -10), S(121, 13), S(88, 18), S(74, 43), S(14, 58), S(-32, 39), S(-8, 15), S(45, 8), S(143, -6), S(90, -2), S(81, 13), S(65, 37), S(34, 49), S(44, 2), S(246, -102), S(84, 3), S(53, -21), S(95, -22), S(159, -29), S(49, 11), S(110, 13), S(230, -50)},
                {S(-23, -15), S(-23, -15), S(-23, -15), S(-23, -15), S(-23, -15), S(-23, -15), S(-23, -15), S(-23, -15), S(-108, 10), S(12, -16), S(7, -33), S(-89, 1), S(-98, -15), S(-70, -12), S(-81, 0), S(-131, 10), S(-100, 2), S(-18, -9), S(-25, -29), S(-66, -17), S(-55, -18), S(-79, -19), S(-55, -12), S(-115, -8), S(-127, 15), S(-45, 2), S(-27, -26), S(-29, -30), S(-50, -23), S(-60, -22), S(-50, -14), S(-110, -4), S(-47, 4), S(-20, -8), S(-27, -24), S(-31, -33), S(-34, -18), S(-52, -13), S(-36, 4), S(-82, 10), S(59, 17), S(96, 6), S(73, -16), S(20, -14), S(49, 27), S(-18, 24), S(29, 46), S(-55, 68), S(560, -1), S(209, 34), S(381, -6), S(356, -30), S(55, 122), S(136, 108), S(252, 81), S(59, 127), S(-23, -15), S(-23, -15), S(-23, -15), S(-23, -15), S(-23, -15), S(-23, -15), S(-23, -15), S(-23, -15)},
                {S(-163, -92), S(-6, -70), S(-14, -37), S(-52, 2), S(-54, -30), S(-119, -15), S(-116, -34), S(-91, -52), S(-63, -9), S(90, -62), S(36, -15), S(11, -12), S(1, -20), S(28, -21), S(25, -36), S(-109, -20), S(-13, -39), S(27, -18), S(49, -1), S(53, 16), S(70, 5), S(0, 13), S(-6, -17), S(-66, 30), S(9, 15), S(17, 13), S(61, 36), S(53, 26), S(31, 38), S(24, 31), S(36, -1), S(-50, 2), S(96, -10), S(80, -4), S(143, 27), S(94, 39), S(92, 34), S(30, 40), S(41, 26), S(-33, 16), S(19, -18), S(100, -7), S(113, -5), S(107, 35), S(99, 30), S(70, 32), S(3, 36), S(34, 26), S(42, -43), S(87, -12), S(27, -8), S(-5, 16), S(-24, 39), S(162, 2), S(-76, 28), S(-10, 6), S(-364, -28), S(-53, -12), S(40, -5), S(-36, -6), S(0, 23), S(-207, 41), S(-45, 20), S(-330, 17)},
                {S(-91, -10), S(-3, -9), S(-59, -10), S(-94, 13), S(-14, -12), S(-47, -12), S(28, -28), S(-73, -13), S(-36, 10), S(28, -21), S(1, 2), S(2, 1), S(-35, 9), S(-5, -8), S(-27, 4), S(-21, -9), S(-74, 16), S(-12, 9), S(37, -1), S(11, 3), S(30, 7), S(13, 12), S(3, -4), S(-33, 7), S(-64, 11), S(20, -3), S(27, 10), S(75, 4), S(90, 10), S(23, 21), S(4, 13), S(45, -12), S(-31, -2), S(47, 5), S(111, -7), S(72, 21), S(68, 16), S(24, 19), S(37, 10), S(-17, 14), S(22, -10), S(162, -30), S(47, -2), S(62, 4), S(36, 23), S(35, 9), S(15, 16), S(-23, -3), S(-3, -34), S(-22, -12), S(160, -37), S(-1, 4), S(55, -13), S(20, 15), S(18, 3), S(-154, 10), S(55, -26), S(-163, -2), S(-87, -12), S(-73, 2), S(-116, 20), S(-32, -16), S(-17, 7), S(-56, -4)},
                {S(-99, -18), S(-37, -20), S(2, -27), S(9, -28), S(-19, -27), S(-45, -22), S(-56, -17), S(-56, -14), S(-45, -28), S(-67, -9), S(41, -54), S(-12, -31), S(-37, -16), S(-95, -20), S(-61, -5), S(-118, 9), S(-63, -16), S(54, -47), S(-34, -14), S(-24, -14), S(-36, -9), S(-112, 20), S(-76, -13), S(-83, -2), S(-121, 19), S(14, -4), S(16, -5), S(10, -16), S(-59, 20), S(-89, 30), S(-116, 22), S(-71, 0), S(-46, 23), S(-10, 14), S(54, 12), S(45, 14), S(15, 12), S(-14, 35), S(-58, 34), S(-77, 32), S(11, 20), S(159, -13), S(20, 29), S(66, 10), S(65, 7), S(42, 20), S(48, 5), S(-21, 33), S(120, -13), S(138, -13), S(184, -25), S(181, -28), S(122, 4), S(72, 12), S(-2, 28), S(-5, 25), S(97, -11), S(14, 22), S(116, -13), S(15, 27), S(64, 0), S(-57, 36), S(76, 6), S(49, 11)},
                {S(-39, -20), S(-35, -169), S(-9, -162), S(-72, -11), S(-8, -68), S(-77, 12), S(-55, 33), S(7, -34), S(-76, 25), S(-17, -105), S(41, -95), S(4, -60), S(-6, -43), S(7, -13), S(-21, -22), S(-25, -117), S(-35, -47), S(32, -106), S(-2, -5), S(-10, -22), S(-12, 7), S(-12, 21), S(-15, -32), S(-5, -106), S(-1, -15), S(4, 28), S(34, -6), S(5, 47), S(16, -3), S(11, -29), S(-28, -28), S(-45, -31), S(40, 0), S(49, -2), S(-35, 135), S(94, 26), S(-10, 54), S(31, -4), S(-31, 63), S(-24, -71), S(119, -40), S(99, 14), S(8, 107), S(-37, 84), S(9, 75), S(-14, 57), S(-61, 43), S(-43, 24), S(98, 59), S(64, 5), S(-25, 77), S(26, 108), S(2, 76), S(5, 10), S(-67, 59), S(-46, -19), S(47, -2), S(1, 11), S(-15, 71), S(209, -55), S(27, 65), S(-79, 103), S(72, 11), S(-70, 34)},
        },
        {
                {S(108, -82), S(153, -95), S(101, -71), S(5, -43), S(-120, -39), S(-200, -9), S(-81, -40), S(-94, -65), S(95, -41), S(89, -37), S(43, -20), S(32, -15), S(-178, 16), S(-165, 13), S(-101, -10), S(-105, -21), S(5, -6), S(55, -6), S(4, 7), S(-15, 15), S(-198, 41), S(-194, 31), S(-160, 14), S(-180, 3), S(21, 2), S(33, 16), S(24, 22), S(0, 35), S(-199, 51), S(-193, 46), S(-198, 35), S(-195, 13), S(72, 4), S(52, 37), S(43, 35), S(79, 30), S(-137, 51), S(-102, 45), S(-161, 56), S(-170, 29), S(135, -2), S(147, 28), S(87, 31), S(88, 34), S(-15, 21), S(-24, 44), S(-76, 57), S(-130, 42), S(141, -17), S(208, -3), S(213, -8), S(86, 16), S(-12, 15), S(-30, 37), S(-67, 49), S(-54, 3), S(383, -134), S(295, -77), S(282, -66), S(201, -50), S(74, -29), S(-37, 9), S(29, 9), S(205, -67)},
                {S(27, -38), S(27, -38), S(27, -38), S(27, -38), S(27, -38), S(27, -38), S(27, -38), S(27, -38), S(-44, -14), S(54, -33), S(1, -41), S(-78, -16), S(-116, -8), S(-77, -13), S(-95, 3), S(-148, 2), S(-40, -14), S(23, -21), S(-23, -34), S(-53, -29), S(-69, -20), S(-56, -29), S(-76, -11), S(-145, -10), S(-62, 11), S(-14, 7), S(-28, -17), S(-24, -33), S(-47, -29), S(-34, -37), S(-54, -24), S(-121, -18), S(-65, 52), S(-4, 34), S(-25, 9), S(-33, -15), S(-10, -36), S(-2, -51), S(-33, -25), S(-97, -21), S(-23, 133), S(-21, 147), S(-8, 119), S(-25, 58), S(47, -17), S(59, -41), S(64, -47), S(-80, 1), S(193, 167), S(331, 163), S(180, 203), S(165, 130), S(138, 62), S(224, -23), S(-44, 16), S(-35, 15), S(27, -38), S(27, -38), S(27, -38), S(27, -38), S(27, -38), S(27, -38), S(27, -38), S(27, -38)},
                {S(-90, -93), S(-81, -5), S(-35, -27), S(-90, -3), S(-86, 4), S(-70, -36), S(-87, -21), S(-67, -48), S(-41, -39), S(7, -26), S(7, -6), S(-25, 17), S(-25, 3), S(-23, -1), S(-16, -22), S(-119, -9), S(-34, -25), S(-4, 8), S(36, 10), S(27, 21), S(42, 26), S(-5, 19), S(-10, 0), S(-38, -23), S(18, -7), S(-1, 20), S(49, 41), S(22, 49), S(58, 34), S(33, 34), S(49, 12), S(-23, -5), S(-13, 19), S(47, 23), S(77, 41), S(133, 33), S(68, 42), S(110, 42), S(21, 14), S(40, -18), S(-40, 17), S(94, 8), S(113, 19), S(104, 34), S(130, 23), S(135, 19), S(116, -13), S(22, -19), S(-57, 8), S(-44, 40), S(59, 14), S(91, 7), S(53, 21), S(46, -6), S(-24, -25), S(-86, -2), S(-132, -74), S(-112, -2), S(-14, 29), S(-18, 1), S(43, -19), S(-280, 21), S(24, -70), S(-83, -129)},
                {S(28, -58), S(28, -36), S(-43, -19), S(-67, -3), S(-46, -12), S(-44, -27), S(-54, -23), S(-151, 12), S(35, -17), S(66, -40), S(43, -13), S(-7, 3), S(-24, 5), S(-34, 11), S(-51, -1), S(-63, -6), S(5, -22), S(40, -4), S(53, 5), S(13, 7), S(10, 22), S(-13, 14), S(-26, -1), S(-11, -16), S(1, 11), S(32, 2), S(35, 15), S(63, 11), S(34, 13), S(-4, 26), S(14, 16), S(-22, 7), S(-1, -1), S(20, 29), S(80, 11), S(81, 14), S(56, 18), S(65, 2), S(12, 15), S(-27, -2), S(-32, 11), S(37, 13), S(38, 14), S(66, 8), S(80, 7), S(52, 20), S(105, -23), S(30, -13), S(-43, -2), S(-1, 19), S(-5, 10), S(14, 4), S(28, 7), S(100, -16), S(69, -40), S(-31, -23), S(-220, 36), S(-143, 34), S(-83, 16), S(-60, 7), S(29, -4), S(-200, 3), S(43, -25), S(1, -29)},
                {S(-128, 15), S(-44, -7), S(-8, -26), S(-21, -14), S(-39, -11), S(-47, -26), S(-38, -32), S(-55, -22), S(-120, 10), S(-34, -13), S(-24, -5), S(-39, -14), S(-42, -21), S(-62, -9), S(-74, -23), S(-72, -28), S(-79, 0), S(-71, 6), S(-51, -3), S(-48, 2), S(-73, 10), S(-41, -8), S(-58, -23), S(-73, -32), S(-46, 10), S(-8, 4), S(-29, 9), S(-34, 18), S(-53, 13), S(-49, 5), S(-90, 4), S(-63, -14), S(37, 9), S(-31, 41), S(-6, 27), S(9, 15), S(0, 13), S(-2, 14), S(-32, 9), S(-18, -2), S(25, 17), S(4, 31), S(9, 32), S(32, 22), S(54, 7), S(62, 8), S(28, 7), S(-8, 21), S(33, 23), S(80, 11), S(125, 4), S(61, 18), S(38, 23), S(166, -23), S(110, 0), S(46, -5), S(244, -84), S(189, -31), S(104, 5), S(62, 7), S(80, 0), S(127, -12), S(24, 15), S(60, -2)},
                {S(-108, -158), S(-42, -201), S(1, -145), S(-31, -72), S(-6, -85), S(-70, -40), S(-45, -60), S(-39, -60), S(-91, 12), S(-43, -31), S(36, -114), S(27, -69), S(16, -65), S(23, -77), S(14, -68), S(-15, -52), S(-35, -70), S(-12, -15), S(26, -3), S(-4, 14), S(26, -33), S(7, -30), S(16, -65), S(17, -48), S(-34, 0), S(-20, 15), S(-10, 37), S(-4, 75), S(6, 30), S(10, 15), S(-7, -8), S(48, -61), S(-34, -6), S(-35, 78), S(13, 88), S(6, 80), S(40, 68), S(22, 24), S(4, 34), S(6, 1), S(-37, 18), S(-22, 82), S(-19, 107), S(17, 73), S(91, 52), S(112, 17), S(93, -29), S(86, -38), S(-70, 20), S(-61, 68), S(-35, 100), S(-1, 84), S(-38, 107), S(96, 12), S(48, 0), S(92, -23), S(-91, 66), S(-98, 74), S(8, 57), S(96, 7), S(-76, 105), S(33, 60), S(-32, 85), S(128, -36)},
        },
        {
                {S(-35, -41), S(16, -70), S(-22, -53), S(-103, -35), S(7, -44), S(-64, -28), S(51, -55), S(34, -79), S(-38, -24), S(-32, -25), S(-65, -15), S(-72, -15), S(-40, 9), S(-24, -1), S(15, -16), S(6, -25), S(-99, -10), S(-52, -9), S(-78, -1), S(-96, 6), S(-76, 37), S(-42, 19), S(-12, 11), S(-57, 1), S(-93, -16), S(-55, 1), S(-64, 9), S(-87, 22), S(-78, 52), S(-39, 43), S(-34, 35), S(-58, 15), S(-36, -13), S(-27, 12), S(-37, 17), S(-3, 14), S(-31, 63), S(-1, 57), S(-33, 64), S(-57, 45), S(-23, -1), S(50, 10), S(13, 9), S(22, 12), S(70, 41), S(84, 54), S(2, 75), S(-92, 64), S(10, -12), S(66, -4), S(95, -14), S(20, -5), S(98, 31), S(110, 49), S(0, 65), S(97, -2), S(249, -125), S(103, -47), S(108, -54), S(81, -47), S(71, -12), S(11, 18), S(179, -13), S(182, -55)},
                {S(8, -33), S(8, -33), S(8, -33), S(8, -33), S(8, -33), S(8, -33), S(8, -33), S(8, -33), S(-125, 17), S(-103, 20), S(-74, -13), S(-110, 0), S(-48, -19), S(9, -27), S(68, -34), S(-7, -27), S(-123, 2), S(-94, 5), S(-78, -20), S(-66, -11), S(-22, -27), S(-30, -27), S(44, -27), S(2, -31), S(-111, -5), S(-65, -13), S(-61, -28), S(-34, -33), S(-33, -26), S(-26, -20), S(8, -8), S(-11, -14), S(-79, -9), S(-30, -22), S(-45, -41), S(-15, -47), S(-32, -8), S(-25, -3), S(28, 12), S(-2, 15), S(-35, 13), S(20, -23), S(59, -53), S(-1, -28), S(12, 39), S(-18, 79), S(38, 103), S(-3, 119), S(205, 17), S(11, 35), S(90, -4), S(137, -16), S(278, 96), S(81, 210), S(162, 223), S(131, 193), S(8, -33), S(8, -33), S(8, -33), S(8, -33), S(8, -33), S(8, -33), S(8, -33), S(8, -33)},
                {S(-130, -17), S(-65, -53), S(-95, -18), S(-80, -11), S(-58, -11), S(-48, -18), S(-52, -48), S(-111, -110), S(-83, -30), S(-60, 20), S(-41, 15), S(-1, 2), S(-15, 11), S(0, -5), S(-6, -5), S(-88, -8), S(-37, -19), S(5, 8), S(-5, 22), S(32, 34), S(30, 31), S(16, 8), S(0, 16), S(-78, -16), S(17, -13), S(-10, 28), S(37, 41), S(51, 32), S(35, 44), S(51, 27), S(48, 3), S(-1, -1), S(51, -14), S(25, 16), S(93, 38), S(85, 33), S(95, 39), S(65, 45), S(35, 21), S(3, 14), S(22, -29), S(76, -7), S(158, -1), S(171, 10), S(144, 10), S(65, 30), S(30, 30), S(-8, -9), S(-31, -28), S(18, 3), S(76, -26), S(25, 5), S(51, 17), S(152, -10), S(-8, 27), S(-50, -8), S(-209, -74), S(3, -46), S(-28, -34), S(-101, 5), S(-131, 7), S(-46, 34), S(85, -37), S(-176, -21)},
                {S(-112, 6), S(-99, 28), S(-63, -16), S(-54, 2), S(-41, -14), S(-22, -32), S(29, -52), S(-22, -51), S(-60, 14), S(-41, 8), S(-18, 8), S(-36, 0), S(-2, -2), S(17, -9), S(55, -24), S(-16, -28), S(-42, -3), S(-29, 22), S(-33, 26), S(5, 11), S(12, 14), S(40, 0), S(37, -9), S(25, -2), S(-27, 0), S(5, 11), S(-8, 22), S(28, 15), S(75, 12), S(40, 9), S(16, 6), S(-6, -2), S(11, -9), S(24, 19), S(66, -13), S(80, 5), S(79, 12), S(37, 33), S(9, 20), S(-16, 9), S(23, -14), S(72, -16), S(41, 0), S(93, -8), S(49, 15), S(26, 22), S(19, 24), S(-24, 13), S(-24, -46), S(118, -34), S(73, -22), S(42, 11), S(23, 2), S(1, 13), S(-30, 29), S(-43, 11), S(-50, -20), S(-62, -40), S(-46, -19), S(-91, 5), S(-60, 16), S(-68, 16), S(-18, 11), S(-7, -17)},
                {S(-42, -27), S(-18, -26), S(-5, -26), S(-23, -12), S(-22, -13), S(-31, -18), S(-56, -14), S(-81, -16), S(-86, -6), S(-27, -32), S(-14, -24), S(-37, -20), S(-24, -13), S(-29, -18), S(-56, -3), S(-164, 8), S(-96, -3), S(-58, -15), S(-38, -14), S(-43, -14), S(-38, -4), S(-67, 6), S(-23, -18), S(-77, -5), S(-67, 3), S(-32, -1), S(-31, 3), S(2, -5), S(-37, 14), S(-53, 10), S(-66, 5), S(-71, 15), S(-47, 21), S(-22, 13), S(-11, 13), S(31, 5), S(33, 8), S(6, 24), S(-45, 33), S(-11, 3), S(14, 17), S(46, 11), S(67, 7), S(58, 5), S(91, -11), S(28, 21), S(44, 17), S(0, 16), S(39, 10), S(38, 13), S(127, -13), S(134, -9), S(83, 7), S(60, 8), S(54, 17), S(81, -2), S(38, 17), S(79, 11), S(137, -12), S(82, 5), S(78, 2), S(66, 9), S(29, 21), S(106, -7)},
                {S(-53, -36), S(-68, -42), S(-62, -42), S(-21, -118), S(-61, -54), S(-68, -45), S(-27, -41), S(-96, -148), S(-41, -39), S(-10, -55), S(8, -79), S(-52, 5), S(-17, -49), S(6, -74), S(-51, -25), S(-111, -52), S(-22, 0), S(-5, -31), S(-26, 4), S(-19, 9), S(-30, 36), S(-8, 15), S(-34, 1), S(-50, -69), S(0, -33), S(-37, 8), S(-14, 18), S(-41, 63), S(-34, 62), S(-58, 63), S(-21, 19), S(-48, 46), S(-24, -4), S(-6, 37), S(-12, 54), S(16, 72), S(-18, 79), S(-5, 59), S(-62, 72), S(-43, 35), S(43, -24), S(63, -11), S(138, -20), S(19, 91), S(34, 41), S(27, 33), S(1, 59), S(-48, 27), S(88, -29), S(46, 25), S(86, 19), S(100, 10), S(46, 37), S(-30, 91), S(-53, 79), S(-38, 5), S(17, -5), S(88, -12), S(109, -7), S(297, -104), S(90, 12), S(120, -18), S(45, -6), S(37, -15)},
        },
        {
                {S(97, -36), S(171, -68), S(133, -51), S(-5, -24), S(-116, -51), S(-203, -17), S(-73, -54), S(-86, -82), S(99, -19), S(79, -15), S(57, -8), S(35, -6), S(-183, 13), S(-169, 9), S(-94, -20), S(-96, -36), S(24, -3), S(69, -2), S(28, 10), S(20, 15), S(-203, 42), S(-208, 32), S(-165, 10), S(-181, -7), S(36, -11), S(55, 9), S(46, 18), S(19, 31), S(-207, 54), S(-212, 50), S(-219, 36), S(-205, 8), S(93, -8), S(88, 19), S(63, 26), S(102, 19), S(-145, 53), S(-95, 42), S(-183, 61), S(-180, 25), S(97, 5), S(165, 16), S(124, 13), S(126, 17), S(-13, 14), S(-19, 41), S(-68, 51), S(-115, 39), S(148, -9), S(179, 1), S(207, -9), S(120, 0), S(-44, 16), S(-65, 24), S(-50, 37), S(-134, 11), S(370, -118), S(251, -47), S(225, -51), S(196, -43), S(110, -35), S(-36, 5), S(-80, 30), S(236, -68)},
                {S(33, -27), S(33, -27), S(33, -27), S(33, -27), S(33, -27), S(33, -27), S(33, -27), S(33, -27), S(-89, 6), S(-62, 4), S(-73, -10), S(-83, 2), S(-60, 7), S(-8, -10), S(50, -29), S(-35, -33), S(-89, 1), S(-55, -2), S(-70, -18), S(-66, -9), S(-46, -10), S(-31, -18), S(30, -24), S(-22, -33), S(-85, 5), S(-46, -4), S(-48, -26), S(-34, -31), S(-29, -31), S(-20, -26), S(-1, -19), S(-45, -26), S(-74, 23), S(-41, 11), S(-44, -10), S(-24, -32), S(-2, -31), S(-5, -34), S(9, -12), S(-19, -25), S(-70, 88), S(-26, 67), S(-7, 41), S(-12, 19), S(20, -15), S(76, -29), S(69, -9), S(11, 0), S(111, 144), S(122, 154), S(102, 146), S(95, 98), S(149, 39), S(97, 18), S(-70, 68), S(20, 51), S(33, -27), S(33, -27), S(33, -27), S(33, -27), S(33, -27), S(33, -27), S(33, -27), S(33, -27)},
                {S(-106, -80), S(-42, -78), S(-76, -20), S(-54, -15), S(-40, -23), S(-6, -44), S(-30, -68), S(-74, -90), S(-68, -43), S(-55, -10), S(-24, -7), S(-6, -3), S(-9, -1), S(10, -22), S(-19, -18), S(-23, -38), S(-36, -29), S(-8, -1), S(5, 9), S(24, 26), S(36, 24), S(12, 7), S(24, -7), S(-30, -27), S(-15, 2), S(15, 19), S(33, 39), S(23, 51), S(40, 51), S(29, 38), S(32, 17), S(-3, -6), S(-9, 18), S(20, 26), S(53, 43), S(94, 43), S(49, 51), S(88, 47), S(16, 37), S(50, 4), S(-30, 9), S(46, 11), S(86, 24), S(94, 33), S(130, 17), S(187, 14), S(87, 3), S(28, -11), S(-43, -7), S(-12, 20), S(53, 10), S(64, 25), S(71, 11), S(131, -23), S(-4, -15), S(20, -32), S(-219, -5), S(-161, 38), S(-120, 61), S(-58, 24), S(35, 6), S(-169, 18), S(-35, -29), S(-103, -124)},
                {S(-47, -39), S(-19, -23), S(-23, -45), S(-48, -6), S(-37, -9), S(-20, -14), S(5, -43), S(-48, -33), S(7, -24), S(-5, -25), S(12, -13), S(-10, 2), S(0, 7), S(25, -16), S(32, -22), S(11, -48), S(-5, -16), S(20, 1), S(8, 12), S(19, 9), S(20, 19), S(12, 8), S(17, -13), S(13, -21), S(-8, -11), S(8, 14), S(19, 21), S(42, 23), S(41, 21), S(12, 16), S(14, 10), S(5, -16), S(-9, 3), S(16, 19), S(42, 12), S(74, 20), S(52, 22), S(56, 14), S(13, 22), S(-2, 5), S(-8, 2), S(40, 8), S(33, 14), S(65, 1), S(85, 8), S(80, 16), S(71, 4), S(34, -1), S(-34, 3), S(11, 7), S(15, 14), S(-14, 12), S(22, 1), S(54, -4), S(-2, -5), S(-4, -20), S(-90, 4), S(-121, 25), S(-112, 18), S(-126, 28), S(-115, 13), S(-189, 23), S(13, -9), S(-24, -5)},
                {S(-48, -20), S(-45, -17), S(-35, 0), S(-24, -3), S(-21, -18), S(-30, -25), S(-22, -24), S(-55, -48), S(-81, -14), S(-59, -13), S(-45, -8), S(-36, -13), S(-29, -23), S(-20, -37), S(9, -47), S(-59, -29), S(-77, -8), S(-68, 1), S(-59, -2), S(-42, -7), S(-36, -14), S(-39, -19), S(8, -42), S(-35, -43), S(-70, 14), S(-61, 16), S(-56, 23), S(-28, 12), S(-24, -5), S(-46, 4), S(-35, -2), S(-46, -23), S(-50, 30), S(-41, 33), S(-20, 31), S(19, 12), S(23, 6), S(15, 9), S(13, 9), S(-6, 7), S(-15, 31), S(9, 25), S(24, 28), S(37, 15), S(84, -3), S(84, -10), S(106, -6), S(17, 13), S(0, 37), S(17, 34), S(35, 35), S(66, 19), S(50, 21), S(111, 3), S(118, -1), S(107, -14), S(48, 5), S(25, 22), S(45, 20), S(66, 7), S(79, 2), S(44, 16), S(104, -1), S(102, -6)},
                {S(-26, -57), S(-36, -60), S(-32, -43), S(-2, -91), S(-20, -73), S(-31, -111), S(-59, -97), S(-57, -104), S(-33, -45), S(-10, -47), S(0, -55), S(0, -55), S(0, -55), S(12, -88), S(29, -122), S(4, -160), S(-32, -41), S(-9, -42), S(-15, -4), S(-14, -13), S(-3, -16), S(2, -20), S(14, -51), S(5, -76), S(-28, -32), S(-32, 17), S(-15, 15), S(-22, 57), S(-12, 34), S(-14, 34), S(0, 15), S(-1, -13), S(-34, -2), S(-35, 38), S(-23, 54), S(-2, 59), S(3, 76), S(-8, 92), S(-10, 62), S(17, 14), S(-32, -10), S(-34, 40), S(-13, 48), S(-13, 73), S(46, 59), S(97, 45), S(73, 40), S(23, 43), S(-66, 37), S(-61, 73), S(-41, 91), S(-33, 97), S(-30, 121), S(104, 24), S(34, 83), S(97, -22), S(-84, 60), S(-22, 36), S(3, 44), S(10, 48), S(26, 47), S(90, 13), S(194, -52), S(159, -30)},
        },
};
const int passedPawns[] = { S(66, -12), S(64, -10), S(59, 17), S(89, 33), S(133, 31), S(165, -52) };
const int bishopPair = S(36, 64);
const int material[] = { S(9989, 10006), S(147, 136), S(391, 379), S(415, 398), S(556, 717), S(1134, 1312) };


//const int isolatedPawns[] = { S(0, 0), S(0, 0), S(0, 0), S(0, 0), S(0, 0), S(0, 0), S(0, 0), S(0, 0)};
//const int openFiles[] = { S(-4, -14), S(-10, -2), S(47, -1), S(-15, 35)};

/*const int threats[4][5] = {
	{S(0, 0), S(0, 0), S(0, 0), S(0,0), S(0, 0)},	// Knight
	{S(0, 0), S(0, 0), S(0, 0), S(0,0), S(0, 0)},	// Bishop
	{S(0, 0), S(0, 0), S(0, 0), S(0,0), S(0, 0)},	// Rook
	{S(0, 0), S(0, 0), S(0, 0), S(0,0), S(0, 0)}	// Queen
};*/




#define TraceIncr(parameter) trace.parameter[color]++
#define TraceAdd(parameter, count) trace.parameter[color] += count

static Trace eval(BoardState& board) {
	Trace trace{};
	const int moveHalves[4] = {4, 6, 7, 14};
	int score[2] = {S(0, 0), S(0, 0)};

	int bishopCounts[2] = {0,0};
	int gamePhase = 0;
	//cout << "\n";
	int buckets[2] = {kingBuckets[board.kingLocations[1]], kingBuckets[board.kingLocations[2] ^ 56]};

	const uint64_t pawns[] = {board.occupiedBoard[1] & board.pieceBoards[2], board.occupiedBoard[2] & board.pieceBoards[2]};

	const uint64_t allPieces = board.occupiedBoard[1] | board.occupiedBoard[2];

	int pawnAttacks[2] = {(ne(pawns[0]) | nw(pawns[0])), (se(pawns[1]) | sw(pawns[1]))}; 

	/* evaluate each piece */
	for (int sq = 0; sq < 64; ++sq) {
		int piece = board.rawBoard[sq];	
		if (piece != 0) {
			//cout << sq << " " << score[0] << " " << score[1] << "\n";
			int pieceType = (piece % 8 - 1);
			int pieceColor = piece / 8 - 1;
			int pieceIndex = sq ^ (pieceColor == 1 ? 56 : 0);
	
			score[pieceColor] += material[pieceType];
			trace.material[pieceType][pieceColor]++;

			int bucketIndex = 2 * buckets[pieceColor] + buckets[1 - pieceColor];
				
			score[pieceColor] += psts[bucketIndex][pieceType][pieceIndex];
			trace.pst[bucketIndex][pieceType][pieceIndex][pieceColor]++;
			
			gamePhase += gamephaseInc[pieceType];

			if(pieceType == 1 && pieceColor == board.currentPlayer - 1) {	// Passed Pawn Detection
				if (!(passedPawnMasks[pieceColor][sq] & board.pieceBoards[2])) {
					score[pieceColor] += passedPawns[pieceIndex / 8 - 1];
					trace.passedPawns[pieceIndex/8 - 1][pieceColor]++;
				}
				/*if(!(isolatedPawnMasks[sq] & board.pieceBoards[2] & board.occupiedBoard[pieceColor + 1])) {
					score[pieceColor] += isolatedPawns[sq % 8];
					trace.isolatedPawns[sq % 8][pieceColor]++;
				}*/
			}
			if(pieceType == 3) {
				bishopCounts[pieceColor]++;
			}
			/*if(pieceType > 1) {	// Not a Pawn, Mobility Test

				uint64_t mobility = 0ull;

				if(pieceType == 2) {
					mobility = knight(sq, allPieces);
				}
				else if(pieceType == 3) {
					mobility = bishop(sq, allPieces);
				}
				else if(pieceType == 4) {
					mobility = rook(sq, allPieces);
				}
				else if(pieceType == 5) {
					mobility = bishop(sq, allPieces) || rook(sq, allPieces);
				}	

				mobility = (mobility & ~board.occupiedBoard[pieceColor + 1]);
				uint64_t threatBoard = mobility & board.occupiedBoard[2 - pieceColor];

				while(threatBoard != 0ull) {
					int victimSq = popLSB(threatBoard);
					score[pieceColor] += threats[pieceType - 2][board.rawBoard[victimSq] % 8 - 2];
					//trace.threats[pieceType - 2][board.rawBoard[victimSq] % 8 - 2][pieceColor]++;

				
				}					

			}*/
			
		}
	}
	
	for(int col = 0; col < 2; col++) {
		if(bishopCounts[col] == 2) {
			score[col] += bishopPair;
			trace.bishopPair[col]++;
		}
	}

	/* tapered eval */
	int mergedScore = score[0] - score[1];

	//cout << mg_score(mergedScore) << " " << eg_score(mergedScore) << "\n";

	int finalScore = (mg_score(mergedScore) * gamePhase + eg_score(mergedScore) * (24 - gamePhase)) / 24;
	finalScore = finalScore / (board.currentPlayer == 1 ? 1 : -1);
	//cout << finalScore << "\n";

	trace.score = finalScore;
	//cout << finalScore << "\n";
	return trace;	
}

parameters_t SkywallEvalTapered::get_initial_parameters()
{
	parameters_t parameters;

	get_initial_parameter_array_2d(parameters, psts[0], 6, 64);
	get_initial_parameter_array_2d(parameters, psts[1], 6, 64);
	get_initial_parameter_array_2d(parameters, psts[2], 6, 64);
	get_initial_parameter_array_2d(parameters, psts[3], 6, 64);

	get_initial_parameter_array(parameters, passedPawns, 6);	
//	get_initial_parameter_array(parameters, isolatedPawns, 8);	

	//get_initial_parameter_array_2d(parameters, threats, 4, 5);
	//get_initial_parameter_array_2d(parameters, mobilityScores, 4, 28);
	get_initial_parameter_single(parameters, bishopPair);
	
	get_initial_parameter_array(parameters, material, 6);
	return parameters;
}

static coefficients_t get_coefficients(const Trace& trace)
{
	coefficients_t coefficients;
	
	get_coefficient_array_2d(coefficients, trace.pst[0], 6, 64);
	get_coefficient_array_2d(coefficients, trace.pst[1], 6, 64);
	get_coefficient_array_2d(coefficients, trace.pst[2], 6, 64);
	get_coefficient_array_2d(coefficients, trace.pst[3], 6, 64);
	
	get_coefficient_array(coefficients, trace.passedPawns, 6);
//	get_coefficient_array(coefficients, trace.isolatedPawns, 8);

	//get_coefficient_array_2d(coefficients, trace.threats, 4, 5);

    get_coefficient_single(coefficients, trace.bishopPair);
	get_coefficient_array(coefficients, trace.material, 6);
	
	
	return coefficients;
}

EvalResult SkywallEvalTapered::get_fen_eval_result(const string& fen)
{
	BoardState position;
	set_fen(position, fen);

	const auto trace = eval(position);

	EvalResult result;
	result.coefficients = get_coefficients(trace);
	result.score = trace.score;	
	result.endgame_scale = 1;

	return result;
}

EvalResult SkywallEvalTapered::get_external_eval_result(const chess::Board& board)
{
	throw std::runtime_error("Not implemented");
}

static void print_parameter(std::stringstream& ss, const pair_t parameter)
{
	const auto mg = round_value(parameter[static_cast<int32_t>(PhaseStages::Midgame)]);
	const auto eg = round_value(parameter[static_cast<int32_t>(PhaseStages::Endgame)]);
	
	//const auto mg = parameter[0];
	//const auto eg = parameter[1];

	if (mg == 0 && eg == 0)
	{
		ss << 0;
	}
	else
	{
		ss << "S(" << mg << ", " << eg << ")";
	}
}

static void print_single(std::stringstream& ss, const parameters_t& parameters, int& index, const std::string& name)
{
	ss << "const int " << name << " = ";
	print_parameter(ss, parameters[index]);
	ss << ";" << endl;

	index++;
}

static void print_array(std::stringstream& ss, const parameters_t& parameters, int& index, const std::string& name, const int count)
{
	ss << "const int " << name << "[] = {";
	for (auto i = 0; i < count; i++)
	{
		print_parameter(ss, parameters[index]);
		index++;

		if (i != count - 1)
		{
			ss << ", ";
		}
	}
	ss << "};" << endl;
}

static void print_array_2d(std::stringstream& ss, const parameters_t& parameters, int& index, const std::string& name, const int count1, const int count2)
{
	ss << "const int " << name << "[][" << count2 << "] = {\n";
	for (auto i = 0; i < count1; i++)
	{
		ss << "	{";
		for (auto j = 0; j < count2; j++)
		{
			print_parameter(ss, parameters[index]);
			index++;

			if (j != count2 - 1)
			{
				ss << ", ";
			}
		}
		ss << "},\n";
	}
	ss << "};\n";
}

static void print_array_3d(std::stringstream& ss, const parameters_t& parameters, int& index, const std::string& name, const int count1, const int count2, const int count3)
{
	ss << "const int " << name << "[][" << count2 << "][" << count3 << "] = {\n";
	for (auto i = 0; i < count1; i++)
	{
		ss << "\t{\n";
		for (auto j = 0; j < count2; j++)
		{
			ss << "\t\t{";
			for(auto k = 0; k < count3; k++) {
				print_parameter(ss, parameters[index]);
				index++;

				if (k != count3 - 1)
				{
					ss << ", ";
				}
			}
			ss << "},\n";	
		}
		ss << "\t},\n";
	}
	ss << "};\n";
}

static void rebalance_psts(parameters_t& parameters, const int32_t pst_offset, const int32_t pst_size)
{
	for(auto bucket = 0; bucket < 4; bucket++) {
		for (auto pieceIndex = 0; pieceIndex < 6; pieceIndex++)
		{
			const int pstStart = pst_offset + bucket * 6 * pst_size + pieceIndex * pst_size;
			for (int stage = 0; stage < 2; stage++)
			{
				double sum = 0;
				for (auto i = 0; i < pst_size; i++)
				{
					const auto pstIndex = pstStart + i;
					sum += parameters[pstIndex][stage];
				}
	
				const auto average = sum / pst_size;

				parameters[pieceIndex][stage] += average;

				for (auto i = 0; i < pst_size; i++)
				{
					const auto pstIndex = pstStart + i;
					parameters[pstIndex][stage] -= average;
				}
			}
		}
	}
}


void SkywallEvalTapered::print_parameters(const parameters_t& parameters)
{
	parameters_t parameters_copy = parameters;
	rebalance_psts(parameters_copy, 6, 64);

	int index = 0;
	stringstream ss;
	
	print_array_3d(ss, parameters_copy, index, "psts", 4, 6, 64);
	print_array(ss, parameters_copy, index, "passedPawns", 6);
//	print_array(ss, parameters_copy, index, "isolatedPawns", 8);
	//print_array(ss, parameters_copy, index, "openFiles", 4)

	//print_array_2d(ss, parameters_copy, index, "threats", 4, 5);
	print_single(ss, parameters_copy, index, "bishopPair");
	
	print_array(ss, parameters_copy, index, "material", 6);	

	cout << ss.str() << "\n";
}
