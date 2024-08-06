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
	int isolatedPawns[8][2]{};

	//int threats[4][5][2]{};	
	//int openFiles[4][2]{};
	int bishopPair[2];	
	int material[6][2]{};
};

const int gamephaseInc[6] = {0, 0, 1, 1, 2, 4};
const int kingBuckets[64] = { 0, 0, 0, 0, 1, 1, 1, 1, 0, 0, 0, 0, 1, 1, 1, 1, 0, 0, 0, 0, 1, 1, 1, 1, 0, 0, 0, 0, 1, 1, 1, 1, 0, 0, 0, 0, 1, 1, 1, 1, 0, 0, 0, 0, 1, 1, 1, 1, 0, 0, 0, 0, 1, 1, 1, 1, 0, 0, 0, 0, 1, 1, 1, 1 };


const int psts[][6][64] = {
        {
                {S(-30, -35), S(23, -72), S(-28, -51), S(-127, -39), S(3, -50), S(-81, -18), S(41, -51), S(25, -75), S(-27, -33), S(-34, -22), S(-54, -16), S(-107, -8), S(-63, 8), S(-50, 4), S(13, -18), S(12, -29), S(-85, -18), S(-77, -2), S(-63, 5), S(-88, 13), S(-86, 32), S(-83, 24), S(-51, 9), S(-70, -3), S(-159, -8), S(-27, 0), S(-71, 21), S(-84, 30), S(-90, 45), S(-86, 40), S(-93, 31), S(-89, 8), S(-45, -12), S(0, 15), S(18, 20), S(-17, 35), S(-31, 46), S(-5, 42), S(-60, 53), S(-71, 27), S(-39, 10), S(115, 12), S(144, -4), S(160, 1), S(88, 18), S(74, 43), S(14, 58), S(-32, 39), S(10, -5), S(120, 7), S(121, -7), S(49, -2), S(81, 13), S(65, 37), S(34, 49), S(44, 2), S(196, -113), S(41, -3), S(29, -13), S(139, -33), S(159, -29), S(49, 11), S(110, 13), S(230, -50)},
                {S(-36, 3), S(-36, 3), S(-36, 3), S(-36, 3), S(-36, 3), S(-36, 3), S(-36, 3), S(-36, 3), S(-86, -9), S(2, -26), S(10, -31), S(-61, -1), S(-76, -26), S(-63, -25), S(-76, -18), S(-84, -13), S(-74, -20), S(-17, -27), S(-12, -36), S(-52, -24), S(-38, -28), S(-61, -38), S(-49, -35), S(-77, -29), S(-81, -10), S(-42, -13), S(-22, -35), S(-13, -38), S(-25, -37), S(-33, -41), S(-40, -34), S(-69, -24), S(-7, -17), S(-26, -13), S(-19, -31), S(-4, -41), S(-25, -26), S(-30, -28), S(-34, -16), S(-55, -6), S(31, 16), S(76, 9), S(51, -16), S(70, -21), S(75, 22), S(-16, 27), S(-68, 58), S(-42, 61), S(445, 22), S(192, 50), S(364, 8), S(357, -22), S(43, 113), S(101, 147), S(158, 132), S(71, 135), S(-36, 3), S(-36, 3), S(-36, 3), S(-36, 3), S(-36, 3), S(-36, 3), S(-36, 3), S(-36, 3)},
                {S(-162, -82), S(-40, -55), S(-137, -20), S(-57, 9), S(-2, -27), S(-103, -13), S(-81, -61), S(-91, -46), S(-19, -28), S(98, -53), S(43, -18), S(2, -6), S(-11, -11), S(57, -26), S(54, -35), S(-75, -22), S(-18, -36), S(-10, -4), S(21, 13), S(39, 16), S(66, 12), S(-3, 16), S(-3, -20), S(-53, 18), S(27, 9), S(17, 21), S(41, 39), S(53, 31), S(39, 33), S(34, 29), S(40, 3), S(-60, -13), S(83, -10), S(50, 9), S(115, 27), S(83, 45), S(92, 36), S(24, 42), S(29, 24), S(-11, 12), S(-49, -4), S(88, 1), S(64, 11), S(86, 29), S(109, 31), S(66, 31), S(54, 11), S(13, 5), S(89, -46), S(80, -17), S(22, 5), S(16, 22), S(9, 28), S(122, 5), S(-72, 15), S(-92, 10), S(-439, 14), S(27, -7), S(63, -33), S(-34, 8), S(55, -8), S(-73, 28), S(-44, 17), S(-333, -15)},
                {S(-64, -10), S(-78, 3), S(-33, -13), S(-80, 7), S(-16, -12), S(-66, -11), S(33, -37), S(-34, -17), S(-114, 22), S(11, -15), S(-42, 9), S(-8, 5), S(-24, 11), S(2, -9), S(-23, -1), S(-93, 8), S(-57, 15), S(-37, 18), S(28, 7), S(12, 7), S(24, 10), S(3, 12), S(18, -7), S(-36, 3), S(-68, 11), S(16, 0), S(10, 13), S(86, 0), S(58, 14), S(28, 18), S(11, 6), S(-9, 3), S(19, -1), S(54, 7), S(100, -6), S(80, 17), S(77, 15), S(3, 20), S(35, 11), S(-3, 5), S(25, -10), S(168, -28), S(74, -11), S(65, 3), S(45, 20), S(44, 11), S(21, 20), S(-40, 3), S(-24, -33), S(-47, -10), S(174, -35), S(4, -3), S(2, -2), S(23, 10), S(18, 1), S(-86, -10), S(171, -45), S(-61, -12), S(-98, -13), S(-74, -3), S(-188, 24), S(-11, -15), S(-12, 13), S(-14, -12)},
                {S(-88, -19), S(-46, -12), S(-10, -22), S(-2, -22), S(-14, -30), S(-22, -33), S(-48, -27), S(-53, -16), S(-93, -12), S(-56, -14), S(24, -44), S(-27, -28), S(-56, -17), S(-92, -10), S(-87, -9), S(-129, 2), S(-91, -9), S(10, -37), S(-26, -17), S(-43, -2), S(-28, -13), S(-115, 24), S(-52, -10), S(-78, -6), S(-100, 11), S(-10, 5), S(29, -10), S(-8, -1), S(-46, 15), S(-61, 31), S(-87, 18), S(-82, 20), S(-72, 28), S(-28, 22), S(58, 3), S(4, 22), S(12, 17), S(-25, 30), S(-46, 28), S(-45, 20), S(7, 16), S(170, -23), S(22, 16), S(57, 20), S(59, 7), S(96, 2), S(49, -1), S(15, 18), S(60, 7), S(146, -16), S(135, -8), S(156, -22), S(190, -16), S(104, 10), S(60, 14), S(-10, 32), S(23, 15), S(141, -18), S(97, -4), S(7, 25), S(67, -4), S(-66, 42), S(56, 8), S(90, 5)},
                {S(-31, -2), S(-38, -120), S(1, -112), S(-40, -46), S(11, -96), S(-105, 33), S(-14, 3), S(-89, 39), S(-46, 2), S(-14, -92), S(29, -85), S(-6, -88), S(-19, -46), S(-2, 5), S(-39, 3), S(-125, -64), S(-17, -74), S(22, -81), S(-1, -40), S(-17, -6), S(-2, -17), S(-9, -26), S(-32, -26), S(3, -130), S(31, -57), S(16, 16), S(18, -16), S(-29, 57), S(17, -18), S(3, -15), S(-27, -35), S(-112, 40), S(19, 81), S(30, 66), S(-42, 124), S(56, 46), S(-5, 69), S(4, 5), S(-75, 85), S(-90, -4), S(127, -18), S(174, -23), S(131, 28), S(-10, 56), S(-2, 71), S(-19, 44), S(-31, 25), S(-16, -22), S(107, 9), S(111, -45), S(-20, 88), S(68, 60), S(-3, 88), S(-11, 9), S(-86, 70), S(-14, -48), S(16, -69), S(53, 21), S(59, 75), S(230, -41), S(51, 36), S(-148, 145), S(91, -20), S(-91, 85)},
        },
        {
                {S(76, -85), S(113, -88), S(63, -64), S(-20, -44), S(-102, -37), S(-182, -7), S(-63, -38), S(-76, -63), S(81, -47), S(56, -34), S(14, -17), S(-8, -8), S(-160, 18), S(-147, 15), S(-83, -8), S(-87, -19), S(19, -22), S(23, -2), S(-8, 6), S(-36, 19), S(-180, 43), S(-176, 33), S(-142, 16), S(-162, 5), S(35, -21), S(34, 11), S(39, 18), S(-3, 34), S(-181, 53), S(-175, 48), S(-180, 37), S(-177, 15), S(58, -4), S(80, 24), S(52, 32), S(50, 33), S(-119, 53), S(-84, 47), S(-143, 58), S(-152, 31), S(130, -5), S(135, 20), S(69, 29), S(94, 28), S(3, 23), S(-6, 46), S(-58, 59), S(-112, 44), S(88, -14), S(175, -3), S(201, -16), S(132, -11), S(6, 17), S(-12, 39), S(-49, 51), S(-36, 5), S(305, -126), S(233, -62), S(185, -52), S(212, -57), S(92, -27), S(-19, 11), S(47, 11), S(223, -65)},
                {S(27, -53), S(27, -53), S(27, -53), S(27, -53), S(27, -53), S(27, -53), S(27, -53), S(27, -53), S(-28, -22), S(34, -34), S(7, -36), S(-78, -4), S(-101, -11), S(-75, -17), S(-99, -3), S(-118, -10), S(-19, -28), S(13, -28), S(-13, -33), S(-42, -26), S(-52, -22), S(-53, -34), S(-84, -19), S(-111, -21), S(-39, 0), S(-30, 5), S(-18, -16), S(-22, -29), S(-34, -32), S(-29, -45), S(-59, -33), S(-94, -29), S(-39, 44), S(-21, 36), S(-16, 13), S(-30, -9), S(0, -30), S(5, -55), S(-38, -31), S(-79, -29), S(-47, 147), S(-22, 161), S(-9, 134), S(-3, 74), S(60, -1), S(77, -43), S(42, -39), S(-54, -8), S(150, 197), S(212, 215), S(177, 224), S(166, 154), S(105, 104), S(166, 28), S(-61, 50), S(-27, 38), S(27, -53), S(27, -53), S(27, -53), S(27, -53), S(27, -53), S(27, -53), S(27, -53), S(27, -53)},
                {S(-73, -92), S(-68, -36), S(-52, -8), S(-53, -19), S(-85, 6), S(-47, -37), S(-75, -6), S(-99, -53), S(-57, -45), S(-12, -19), S(16, -6), S(-14, 16), S(-23, 6), S(-31, 12), S(-20, -10), S(-108, -6), S(-53, -8), S(-3, 7), S(16, 17), S(15, 36), S(24, 31), S(-4, 18), S(-11, -1), S(-42, -23), S(-2, 9), S(21, 7), S(51, 39), S(24, 47), S(52, 40), S(34, 36), S(42, 11), S(-17, -4), S(5, 4), S(48, 23), S(68, 41), S(106, 37), S(72, 46), S(117, 34), S(31, 5), S(55, -21), S(-36, 16), S(81, 7), S(101, 24), S(112, 33), S(143, 18), S(161, 12), S(109, -8), S(-12, -6), S(-57, -6), S(-49, 22), S(49, 4), S(122, 5), S(76, 8), S(43, -4), S(12, -40), S(16, -37), S(-186, -77), S(-191, -4), S(-56, 24), S(-47, 6), S(17, -10), S(-292, 35), S(81, -59), S(-48, -98)},
                {S(-12, -51), S(24, -33), S(-33, -26), S(-42, -12), S(-59, -9), S(-52, -26), S(-47, -19), S(-128, 12), S(22, -19), S(46, -24), S(51, -23), S(-16, 2), S(-28, 2), S(-17, 6), S(-44, -3), S(-33, -10), S(-1, -16), S(36, -4), S(28, 16), S(11, 8), S(0, 23), S(-15, 15), S(-15, -10), S(-34, -12), S(-36, 16), S(9, 12), S(30, 15), S(50, 14), S(26, 16), S(0, 25), S(12, 8), S(-23, 0), S(-4, 3), S(12, 27), S(68, 16), S(69, 19), S(67, 18), S(67, 2), S(8, 19), S(-22, -5), S(-27, 8), S(32, 14), S(58, 11), S(51, 8), S(108, 3), S(55, 21), S(119, -26), S(34, -8), S(-58, -3), S(-34, 28), S(0, 5), S(5, 7), S(38, 7), S(83, -15), S(33, -30), S(-13, -29), S(-156, 24), S(-69, 18), S(-48, 7), S(8, 2), S(-52, 6), S(-157, 7), S(38, -32), S(-24, -21)},
                {S(-108, 5), S(-53, -8), S(-15, -23), S(-25, -12), S(-34, -12), S(-35, -25), S(-22, -33), S(-55, -23), S(-127, 11), S(-51, -9), S(-32, -4), S(-44, -11), S(-46, -15), S(-68, -11), S(-49, -31), S(-62, -32), S(-92, 1), S(-92, 16), S(-58, -2), S(-49, -1), S(-73, 9), S(-32, -11), S(-49, -18), S(-58, -34), S(-53, 8), S(-10, 11), S(-70, 26), S(-40, 21), S(-39, 10), S(-39, 5), S(-68, -1), S(-52, -12), S(13, 9), S(-24, 35), S(13, 18), S(9, 14), S(-8, 23), S(22, 4), S(3, -6), S(-7, -3), S(17, 20), S(-16, 31), S(25, 21), S(44, 15), S(32, 15), S(48, 2), S(58, -3), S(17, 7), S(57, 18), S(67, 13), S(77, 16), S(67, 18), S(24, 30), S(143, -14), S(100, 0), S(49, -5), S(201, -63), S(164, -19), S(102, 3), S(40, 16), S(129, -10), S(90, -1), S(72, 2), S(70, -1)},
                {S(-133, -98), S(-69, -184), S(-29, -106), S(-70, -17), S(-25, -76), S(-46, -60), S(-64, -21), S(-6, -106), S(-131, 47), S(7, -108), S(28, -129), S(15, -53), S(-3, -52), S(17, -59), S(0, -48), S(-10, -76), S(-45, -58), S(-5, -49), S(27, -9), S(-3, 6), S(11, -20), S(-6, -7), S(-1, -21), S(14, -40), S(-21, -26), S(-31, 26), S(-6, 28), S(-16, 75), S(7, 31), S(-2, 35), S(-5, 9), S(59, -91), S(-27, -8), S(-21, 46), S(2, 82), S(-1, 62), S(37, 58), S(20, 31), S(9, 44), S(19, -1), S(-8, -12), S(-30, 59), S(-25, 97), S(39, 52), S(90, 44), S(68, 41), S(110, -30), S(94, -46), S(-37, -5), S(-24, 19), S(-15, 76), S(-4, 89), S(-17, 96), S(63, 57), S(31, 14), S(120, -34), S(-114, 69), S(-9, 24), S(-11, 78), S(68, 29), S(-22, 75), S(-25, 105), S(11, 61), S(149, -15)},
        },
        {
                {S(-56, -42), S(-5, -71), S(-43, -54), S(-124, -36), S(-1, -31), S(-73, -13), S(38, -42), S(30, -69), S(-59, -25), S(-53, -26), S(-86, -16), S(-93, -16), S(-34, 18), S(-31, 12), S(6, -2), S(23, -23), S(-120, -11), S(-73, -10), S(-99, -2), S(-117, 5), S(-57, 44), S(-25, 27), S(4, 17), S(-18, 0), S(-114, -17), S(-76, 0), S(-85, 8), S(-108, 21), S(-51, 59), S(-9, 48), S(4, 37), S(-6, 9), S(-57, -14), S(-48, 11), S(-58, 16), S(-24, 13), S(17, 63), S(29, 62), S(-14, 65), S(-48, 48), S(-44, -2), S(29, 9), S(-8, 8), S(1, 11), S(43, 48), S(164, 43), S(81, 66), S(6, 45), S(-11, -13), S(45, -5), S(74, -15), S(-1, -6), S(136, 25), S(112, 45), S(29, 56), S(153, -10), S(228, -126), S(82, -48), S(87, -55), S(60, -48), S(62, -11), S(83, 6), S(147, -8), S(149, -89)},
                {S(27, -39), S(27, -39), S(27, -39), S(27, -39), S(27, -39), S(27, -39), S(27, -39), S(27, -39), S(-116, 1), S(-101, 11), S(-73, -14), S(-82, -10), S(-56, -16), S(-1, -29), S(37, -38), S(0, -36), S(-111, -12), S(-93, -5), S(-68, -22), S(-54, -14), S(-23, -32), S(-29, -32), S(15, -35), S(5, -42), S(-104, -18), S(-64, -22), S(-47, -36), S(-27, -37), S(-31, -31), S(-27, -26), S(-11, -16), S(-10, -23), S(-70, -19), S(-31, -26), S(-21, -49), S(-3, -46), S(-15, -16), S(-23, -2), S(2, 10), S(7, 6), S(-42, 12), S(7, -15), S(53, -43), S(29, -27), S(17, 44), S(-29, 96), S(-2, 126), S(-36, 133), S(147, 50), S(-1, 54), S(118, 9), S(131, 22), S(196, 133), S(88, 221), S(40, 260), S(77, 223), S(27, -39), S(27, -39), S(27, -39), S(27, -39), S(27, -39), S(27, -39), S(27, -39), S(27, -39)},
                {S(-139, -29), S(-57, -51), S(-75, -18), S(-81, -4), S(-67, 4), S(-70, -17), S(-55, -42), S(-80, -100), S(-97, -28), S(-83, 14), S(-34, 13), S(-12, 9), S(-6, 10), S(-8, -5), S(-54, 22), S(-91, -23), S(-39, -18), S(-5, 14), S(-8, 29), S(26, 41), S(10, 40), S(6, 12), S(-5, 14), S(-56, -27), S(25, -10), S(0, 27), S(38, 35), S(31, 44), S(37, 42), S(40, 33), S(47, 7), S(-11, 0), S(44, -13), S(50, 9), S(69, 47), S(108, 29), S(87, 44), S(70, 38), S(23, 23), S(-17, 7), S(-3, -20), S(97, -11), S(163, -1), S(147, 17), S(114, 18), S(103, 23), S(35, 25), S(0, -8), S(-23, -32), S(10, -3), S(45, 0), S(48, -6), S(77, 1), S(137, -15), S(-3, 15), S(-48, -8), S(-317, -41), S(7, -44), S(-43, -30), S(-37, -8), S(-102, -3), S(1, 9), S(121, -29), S(-96, -72)},
                {S(-98, 10), S(-126, 32), S(-66, -16), S(-47, 0), S(-33, -16), S(-31, -26), S(-15, -48), S(-2, -59), S(-52, 21), S(-33, 0), S(-9, 1), S(-27, 1), S(-8, -1), S(38, -19), S(41, -14), S(-13, -26), S(-34, 0), S(-31, 21), S(-31, 20), S(-2, 12), S(8, 16), S(23, 2), S(48, -10), S(12, -11), S(-24, -4), S(7, 2), S(-9, 22), S(22, 16), S(62, 14), S(40, 12), S(17, 6), S(-15, -5), S(3, -7), S(17, 18), S(48, -10), S(79, 5), S(83, 14), S(39, 25), S(5, 25), S(-10, 12), S(17, -15), S(66, -13), S(32, 3), S(103, -9), S(56, 12), S(55, 23), S(42, 16), S(-32, 19), S(-11, -53), S(138, -38), S(57, -16), S(48, 4), S(14, 11), S(4, 15), S(-21, 24), S(-46, 9), S(-119, 0), S(0, -40), S(-51, -19), S(-97, 4), S(-40, 14), S(-87, 22), S(-17, 8), S(13, -14)},
                {S(-55, -21), S(-23, -20), S(-20, -18), S(-32, -4), S(-33, -5), S(-46, -12), S(-71, -5), S(-90, -10), S(-95, -4), S(-23, -33), S(-28, -20), S(-38, -21), S(-30, -15), S(-47, -13), S(-76, 13), S(-171, 7), S(-96, -3), S(-52, -18), S(-52, -10), S(-65, -7), S(-59, 5), S(-79, 15), S(-49, -1), S(-63, -12), S(-76, 7), S(-28, -6), S(-48, 8), S(-11, 0), S(-31, 17), S(-49, 12), S(-78, 11), S(-40, 2), S(-60, 21), S(3, 3), S(-24, 19), S(62, 0), S(22, 9), S(-7, 26), S(-32, 25), S(1, -2), S(10, 14), S(55, 8), S(75, 4), S(76, -7), S(82, -7), S(77, 4), S(76, -2), S(23, 9), S(43, 9), S(58, 4), S(118, -10), S(143, -9), S(67, 13), S(90, 1), S(45, 16), S(98, -10), S(58, 13), S(83, 4), S(120, -4), S(57, 13), S(71, 4), S(80, -1), S(61, 7), S(124, -13)},
                {S(-74, -4), S(-78, -1), S(-69, -28), S(-30, -93), S(-47, -58), S(-70, -51), S(-41, -73), S(-100, -109), S(-17, -57), S(-20, -27), S(-6, -83), S(-50, -7), S(-26, -41), S(0, -102), S(-14, -111), S(-78, -48), S(-33, 12), S(-15, -23), S(-30, 12), S(-27, 13), S(-41, 33), S(-23, 16), S(-50, 23), S(-61, -28), S(-10, -12), S(-17, 5), S(-19, 20), S(-30, 62), S(-45, 73), S(-47, 55), S(-31, 37), S(-45, 39), S(-4, 2), S(3, 43), S(-4, 49), S(15, 70), S(-23, 79), S(-7, 60), S(-52, 39), S(-29, -23), S(25, -9), S(55, -15), S(112, 19), S(45, 58), S(31, 41), S(23, 32), S(7, 44), S(-61, 34), S(105, -42), S(66, 12), S(121, -10), S(36, 60), S(5, 63), S(-29, 93), S(-49, 51), S(-12, -20), S(11, 11), S(63, 20), S(158, -28), S(310, -104), S(104, 2), S(138, -39), S(40, -9), S(43, -27)},
        },
        {
                {S(94, -35), S(168, -67), S(130, -50), S(-8, -23), S(-159, -40), S(-225, -9), S(-106, -43), S(-107, -80), S(96, -18), S(76, -14), S(54, -7), S(32, -5), S(-210, 21), S(-196, 18), S(-118, -14), S(-111, -35), S(21, -2), S(66, -1), S(25, 11), S(17, 16), S(-217, 46), S(-211, 35), S(-169, 11), S(-182, -7), S(33, -10), S(52, 10), S(43, 19), S(16, 32), S(-200, 57), S(-210, 51), S(-200, 35), S(-195, 4), S(90, -7), S(85, 20), S(60, 27), S(99, 20), S(-132, 51), S(-99, 46), S(-148, 53), S(-185, 24), S(94, 6), S(162, 17), S(121, 14), S(123, 18), S(-8, 15), S(15, 35), S(-43, 57), S(-117, 31), S(145, -8), S(176, 2), S(204, -8), S(117, 1), S(-38, 11), S(31, 11), S(-96, 51), S(-120, 3), S(367, -117), S(248, -46), S(222, -50), S(193, -42), S(134, -50), S(-60, 12), S(95, -10), S(160, -84)},
                {S(24, -25), S(24, -25), S(24, -25), S(24, -25), S(24, -25), S(24, -25), S(24, -25), S(24, -25), S(-77, -8), S(-63, -7), S(-64, -15), S(-67, -6), S(-54, 0), S(-8, -17), S(19, -37), S(-23, -49), S(-76, -14), S(-60, -12), S(-55, -27), S(-51, -20), S(-37, -21), S(-28, -27), S(2, -36), S(-19, -46), S(-71, -10), S(-49, -11), S(-43, -33), S(-25, -35), S(-26, -37), S(-20, -36), S(-21, -31), S(-37, -37), S(-59, 10), S(-42, 7), S(-32, -13), S(-13, -34), S(3, -36), S(6, -43), S(-10, -24), S(-16, -35), S(-57, 83), S(-33, 81), S(8, 51), S(12, 28), S(36, -1), S(81, -25), S(45, 2), S(8, 4), S(137, 160), S(93, 189), S(106, 161), S(121, 117), S(136, 75), S(82, 42), S(-67, 88), S(17, 77), S(24, -25), S(24, -25), S(24, -25), S(24, -25), S(24, -25), S(24, -25), S(24, -25), S(24, -25)},
                {S(-98, -87), S(-31, -78), S(-71, -18), S(-44, -14), S(-32, -23), S(-18, -29), S(-35, -60), S(-61, -89), S(-53, -50), S(-47, -6), S(-13, -8), S(-2, -2), S(-4, 1), S(7, -17), S(-13, -22), S(-13, -51), S(-26, -40), S(-3, 0), S(11, 6), S(23, 31), S(28, 30), S(14, 12), S(19, 4), S(-18, -31), S(-12, -7), S(2, 19), S(29, 39), S(21, 54), S(38, 52), S(26, 50), S(39, 22), S(0, -1), S(2, -2), S(23, 24), S(51, 41), S(82, 50), S(48, 57), S(78, 51), S(24, 40), S(45, 9), S(-24, -10), S(39, 9), S(71, 32), S(87, 37), S(127, 22), S(183, 18), S(85, 12), S(39, -14), S(-47, -15), S(-27, 13), S(53, 7), S(62, 26), S(77, 11), S(131, -30), S(-12, -13), S(11, -39), S(-241, 4), S(-157, 29), S(-139, 53), S(-76, 35), S(37, 9), S(-153, 20), S(-60, -27), S(-82, -145)},
                {S(-28, -45), S(-20, -29), S(-24, -47), S(-42, -10), S(-41, -11), S(-26, -16), S(-18, -30), S(-35, -34), S(0, -24), S(0, -27), S(8, -12), S(-11, -3), S(-2, 3), S(16, -17), S(28, -18), S(18, -54), S(-6, -20), S(14, 0), S(4, 14), S(15, 10), S(12, 19), S(10, 10), S(12, -9), S(19, -26), S(-13, -4), S(6, 11), S(16, 21), S(35, 25), S(35, 22), S(9, 15), S(10, 6), S(3, -12), S(-20, 5), S(15, 18), S(35, 11), S(65, 27), S(45, 26), S(50, 17), S(11, 23), S(2, 4), S(-8, 1), S(29, 10), S(42, 15), S(61, 6), S(79, 9), S(90, 17), S(77, 6), S(37, -6), S(-48, -1), S(13, 8), S(5, 14), S(-8, 10), S(7, 9), S(71, -13), S(-8, 0), S(12, -27), S(-54, 1), S(-116, 23), S(-95, 22), S(-136, 33), S(-117, 13), S(-137, 15), S(55, -14), S(-61, 15)},
                {S(-53, -19), S(-47, -17), S(-38, 0), S(-29, -2), S(-29, -14), S(-38, -20), S(-24, -22), S(-47, -53), S(-81, -17), S(-64, -12), S(-49, -7), S(-41, -12), S(-35, -23), S(-26, -35), S(-2, -46), S(-67, -30), S(-79, -9), S(-65, 1), S(-61, -4), S(-42, -7), S(-45, -9), S(-51, -13), S(-1, -35), S(-44, -34), S(-67, 11), S(-63, 19), S(-56, 21), S(-33, 14), S(-34, 3), S(-45, 8), S(-20, -10), S(-36, -17), S(-48, 26), S(-33, 25), S(-13, 25), S(20, 14), S(16, 10), S(16, 8), S(18, 6), S(10, 0), S(-18, 29), S(11, 21), S(19, 25), S(42, 11), S(82, -3), S(91, -5), S(145, -23), S(30, 7), S(5, 32), S(11, 35), S(32, 34), S(53, 23), S(48, 26), S(114, -2), S(99, 1), S(114, -16), S(44, 12), S(45, 18), S(52, 20), S(67, 10), S(72, 9), S(52, 17), S(105, 3), S(110, -5)},
                {S(-22, -62), S(-34, -67), S(-29, -51), S(-5, -96), S(-14, -82), S(-32, -110), S(-55, -102), S(-52, -92), S(-27, -61), S(-9, -57), S(-2, -65), S(-4, -53), S(-3, -57), S(13, -103), S(23, -133), S(-2, -131), S(-29, -47), S(-5, -44), S(-16, -1), S(-13, -17), S(-8, -17), S(-1, -19), S(9, -40), S(7, -67), S(-27, -37), S(-24, 10), S(-19, 15), S(-24, 48), S(-15, 40), S(-11, 36), S(4, 13), S(6, 0), S(-27, -20), S(-34, 29), S(-19, 39), S(-1, 50), S(2, 72), S(-6, 92), S(-1, 65), S(18, 24), S(-34, -16), S(-39, 28), S(-13, 37), S(-4, 57), S(40, 68), S(108, 40), S(85, 57), S(31, 42), S(-59, 27), S(-59, 63), S(-27, 66), S(-41, 97), S(-27, 114), S(129, 6), S(0, 137), S(92, -15), S(-65, 51), S(-15, 34), S(-9, 44), S(16, 48), S(29, 54), S(116, 18), S(130, 17), S(105, 25)},
        },
};
const int passedPawns[] = { S(54, -1), S(56, -2), S(47, 23), S(69, 39), S(98, 39), S(126, -52) };
const int isolatedPawns[] = { S(-14, -3), S(-17, -15), S(-28, -15), S(-38, -19), S(-35, -20), S(-27, -15), S(-25, -8), S(-29, -2) };
const int bishopPair = S(37, 66);
const int material[] = { S(9993, 10005), S(135, 154), S(418, 371), S(438, 397), S(600, 707), S(1299, 1268) };

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

			if(pieceType == 1) {	// Passed Pawn Detection
				if (pieceColor == board.currentPlayer - 1 && !(passedPawnMasks[pieceColor][sq] & board.pieceBoards[2])) {
					score[pieceColor] += passedPawns[pieceIndex / 8 - 1];
					trace.passedPawns[pieceIndex/8 - 1][pieceColor]++;
				}
				if(!(isolatedPawnMasks[sq] & board.pieceBoards[2] & board.occupiedBoard[pieceColor + 1])) {
					score[pieceColor] += isolatedPawns[sq % 8];
					trace.isolatedPawns[sq % 8][pieceColor]++;
				}
			}
			if(pieceType == 3) {
				bishopCounts[pieceColor]++;
			}
			if(pieceType > 1) {	// Not a Pawn, Mobility Test

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

				mobility = mobility & (~board.occupiedBoard[pieceColor + 1]);

				// How many squares can you go to that aren't being attacked by pawns	

			}
			
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
	get_initial_parameter_array(parameters, isolatedPawns, 8);	

	//get_initial_parameter_array(parameters, openFiles, 4);	

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
	get_coefficient_array(coefficients, trace.isolatedPawns, 8);
	//get_coefficient_array(coefficients, trace.openFiles, 4);

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

static void rebalance_psts(parameters_t& parameters, const int32_t matLoc, const int32_t pst_size)
{
	for(auto bucket = 0; bucket < 4; bucket++) {
		for (auto pieceIndex = 0; pieceIndex < 6; pieceIndex++)
		{
			const int pstStart = bucket * 6 * pst_size + pieceIndex * pst_size;
			for (int stage = 0; stage < 2; stage++)
			{
				double sum = 0;
				for (auto i = 0; i < pst_size; i++)
				{
					const auto pstIndex = pstStart + i;
					sum += parameters[pstIndex][stage];
				}
	
				const auto average = sum / pst_size;

				parameters[matLoc + pieceIndex][stage] += average;

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
	rebalance_psts(parameters_copy, 1551, 64);

	int index = 0;
	stringstream ss;
	
	print_array_3d(ss, parameters_copy, index, "psts", 4, 6, 64);
	print_array(ss, parameters_copy, index, "passedPawns", 6);
	print_array(ss, parameters_copy, index, "isolatedPawns", 8);
	//print_array(ss, parameters_copy, index, "openFiles", 4);

	//print_array_2d(ss, parameters_copy, index, "threats", 4, 5);
	print_single(ss, parameters_copy, index, "bishopPair");
	
	print_array(ss, parameters_copy, index, "material", 6);	

	cout << ss.str() << "\n";
}
