// Copyright [2015] <Chafic Najjar>

#ifndef SRC_PADDLE_H_
#define SRC_PADDLE_H_

#include <vector>

class Ball;

class Invader {
public:
    Invader(bool right)
        : right(right) { }

    double x, y;
    double dx = -1, dy = 0;
    int type;
    bool right;

    void reset();

    void step() {
        x += dx;
        y += dy;
    }
};

class Paddle {
public:
    Paddle(int x, int y, bool right);

    // Paddle position
    int x;
    int y;
    bool right;

    // Paddle dimensions
    static const int HEIGHT;
    static const int WIDTH;

    std::vector<Invader> invaders;

    int get_x() const;
    int get_y() const;
    void set_x(int new_x);
    void set_y(int new_y);
    void add_to_y(int new_y);
    int predict(Ball* ball);
    void AI_left(Ball* ball);
    void AI_right(Ball* ball);

    void check_invaders();
};

#endif // SRC_PADDLE_H_
