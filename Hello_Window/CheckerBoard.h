#pragma once

#include <vector>
#include <glm/glm.hpp>
#include "Object.h"
#include "Checker.h"
#include "shader.h"

class CheckersBoard {
public:
    static constexpr int SIZE = 8;
    glm::vec3 origin;
    float cellSize;
    CheckersBoard(Model whiteModel,
        Model blackModel,
        Model highlightModel,
        glm::vec3 origin,
        float cellSize,
        float height = 0.0f);
    ~CheckersBoard();

    // Process click on board cell
    void onCellClick(int row, int col);
    // Draw all checkers and highlights
    void render(Shader& shader);

private:
    Checker* board[SIZE][SIZE] = { {} };
    Model highlightModel;
    float height;
    std::vector<Object*> highlights;
    Checker* selectedChecker = nullptr;
    int selectedRow = -1, selectedCol = -1;

    void clearHighlights();
    std::vector<std::pair<int, int>> calculateMoves(int row, int col) const;
    bool isInside(int r, int c) const { return r >= 0 && r < SIZE && c >= 0 && c < SIZE; }
    glm::vec3 cellPosition(int row, int col) const {
        return origin + glm::vec3(col * cellSize, height, row * cellSize);
    }
};

CheckersBoard::CheckersBoard(Model whiteModel,
    Model blackModel,
    Model highlightModel_,
    glm::vec3 origin_,
    float cellSize_,
    float height_)
    : highlightModel(highlightModel_), origin(origin_), cellSize(cellSize_), height(height_) {
    for (int r = 0; r < SIZE; ++r)
        for (int c = 0; c < SIZE; ++c)
            board[r][c] = nullptr;
    // Setup initial pieces
    for (int r = 0; r < 3; ++r)
        for (int c = 0; c < SIZE; ++c)
            if ((r + c) % 2 == 1) board[r][c] = new Checker("Black", blackModel, cellPosition(r, c));
    for (int r = 5; r < SIZE; ++r)
        for (int c = 0; c < SIZE; ++c)
            if ((r + c) % 2 == 1) board[r][c] = new Checker("White", whiteModel, cellPosition(r, c));
}

CheckersBoard::~CheckersBoard() {
    for (int r = 0; r < SIZE; ++r)
        for (int c = 0; c < SIZE; ++c)
            delete board[r][c];
    clearHighlights();
}

void CheckersBoard::onCellClick(int row, int col) {
    if (selectedChecker) {
        for (std::pair<int,int> a : calculateMoves(selectedRow, selectedCol)) {
            int r = a.first;
            int c = a.second;
            if (r == row && c == col) {
                board[row][col] = selectedChecker;
                board[selectedRow][selectedCol] = nullptr;
                selectedChecker->newPos(cellPosition(row, col));
                // Crown if reached last rank
                if ((!selectedChecker->getKing() && (row == 0 || row == SIZE - 1))) selectedChecker->setKing();
                clearHighlights(); selectedChecker = nullptr;
                return;
            }
        }
    }
    // select new
    clearHighlights();
    if (!isInside(row, col) || !board[row][col]) { 
        std::cout << "error choose\n";
        selectedChecker = nullptr;
        return; 
    }
    selectedChecker = board[row][col];
    selectedRow = row; selectedCol = col;
    for (auto m : calculateMoves(row, col)) {
        Object* hl = new Object("Highlight", highlightModel, cellPosition(m.first, m.second));
        highlights.push_back(hl);
    }
}

std::vector<std::pair<int, int>> CheckersBoard::calculateMoves(int row, int col) const {
    std::vector<std::pair<int, int>> moves;
    Checker* chk = board[row][col]; if (!chk) return moves;
    bool forwardOnly = !chk->getKing();
    int dirs[2] = { -1,1 };
    for (int dr : dirs) {
        if (forwardOnly && ((chk->name.rfind("White", 0) == 0 && dr > 0) || (chk->name.rfind("Black", 0) == 0 && dr < 0))) continue;
        for (int dc : dirs) {
            int nr = row + dr, nc = col + dc;
            if (isInside(nr, nc) && !board[nr][nc]) moves.emplace_back(nr, nc);
            int jr = row + 2 * dr, jc = col + 2 * dc;
            if (isInside(jr, jc) && board[nr][nc] && board[nr][nc]->getKing() != chk->getKing() && !board[jr][jc]) moves.emplace_back(jr, jc);
        }
    }
    return moves;
}

void CheckersBoard::clearHighlights() {
    for (auto* o : highlights) delete o;
    highlights.clear();
}

void CheckersBoard::render(Shader& shader) {
    for (int r = 0;r < SIZE;++r) for (int c = 0;c < SIZE;++c)
        if (board[r][c]) board[r][c]->model.Draw(shader);
    for (auto* h : highlights) h->model.Draw(shader);
}

/* Application integration (modify Application.h/.cpp):

// In Application.h:
#include "CheckersBoard.h"
CheckersBoard* board_;

// In loadResources() instead of manual objects_ filling:
Model whiteM("../resources/objects/checker_white/shashka v4.obj");
Model blackM("../resources/objects/checker_black/shashka v4.obj");
Model hlM("../resources/objects/highlight/square.obj");
board_ = new CheckersBoard(
    whiteM, blackM, hlM,
    glm::vec3(-7.0f,0.1f,-7.0f), // adjust to match your grid spacing
    2.0f, 0.1f);

// In render():
for(auto light: ...){}
board_->render(*shader_);

// In onMouseButton():
if(button==GLFW_MOUSE_BUTTON_LEFT && action==GLFW_PRESS && !cursorLocked_) {
    int row,col;
    if(screenToBoardCoords(x,y,row,col)) board_->onCellClick(row,col);
}

*/
