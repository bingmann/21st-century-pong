// Copyright [2015] <Chafic Najjar>

#include "src/paddle.h"

#include "src/ball.h"
#include "src/pong.h"

#include <iostream>
#include <random>

const int Paddle::HEIGHT = 120;
const int Paddle::WIDTH = 16;

namespace {
std::random_device rd;
std::mt19937 gen(rd());
std::normal_distribution<> dist(0, 0.1);
}

void Invader::reset()
{
    x = gen() % Pong::SCREEN_WIDTH;
    y = gen() % Pong::SCREEN_HEIGHT;
    dx = std::abs(dist(gen) * 8.0 + 4.0) + 1.0;
    if (right)
        dx = -dx;
    dy = 0;
    type = gen() % 4;
}

Paddle::Paddle(int new_x, int new_y, bool new_right)
{
    x = new_x;
    y = new_y;
    right = new_right;

    invaders.emplace_back(new_right);
    invaders.emplace_back(new_right);
    invaders.emplace_back(new_right);
    invaders.emplace_back(new_right);
    invaders.emplace_back(new_right);
    invaders.emplace_back(new_right);
    invaders.emplace_back(new_right);
    invaders.emplace_back(new_right);
    invaders.emplace_back(new_right);
    invaders.emplace_back(new_right);
    invaders.emplace_back(new_right);
    invaders.emplace_back(new_right);
}

int Paddle::get_x() const { return x; }

int Paddle::get_y() const { return y; }

void Paddle::set_x(int new_x) {
    x = new_x;
}

void Paddle::set_y(int new_y) {
    y = new_y;

    // Paddle shouldn't be allowed to go above or below the screen.
    if (y < 0)
        y = 0;
    else if (y + HEIGHT > Pong::SCREEN_HEIGHT)
        y = Pong::SCREEN_HEIGHT - HEIGHT;
}

void Paddle::add_to_y(int new_y) {
    y += new_y;

    // Paddle shouldn't be allowed to go above or below the screen.
    if (y < 0)
        y = 0;
    else if (y + HEIGHT > Pong::SCREEN_HEIGHT)
        y = Pong::SCREEN_HEIGHT - HEIGHT;
}

// Imprecise prediction of ball position on the y-axis.
int Paddle::predict(Ball* ball) {
    // Find slope.
    float slope = static_cast<float>(ball->y - ball->y + ball->dy) /
                  (ball->x - ball->x + ball->dx);

    // Distance between ball and paddle.
    int paddle_distance = ball->x - x;

    // Prediction without taking into consideration upper and
    // bottom wall collisions.
    int predicted_y = abs(slope * -(paddle_distance) + ball->y);

    // Calculate number of reflexions.
    int number_of_reflexions = predicted_y / Pong::SCREEN_HEIGHT;

    // Predictions taking into consideration upper and bottom
    // wall collisions.

    // Even number of reflexions.
    if (number_of_reflexions % 2 == 0)
        predicted_y = predicted_y % Pong::SCREEN_HEIGHT;
    // Odd number of reflexions.
    else
        predicted_y = Pong::SCREEN_HEIGHT - (predicted_y % Pong::SCREEN_HEIGHT);

    // add some normal randomness to where to hit paddle
    predicted_y += Paddle::HEIGHT * dist(gen);

    return predicted_y;
}

// Basic AI movement.
void Paddle::AI_left(Ball* ball) {
    // Ball on the left 3/5th side of the screen and going left.
    if (ball->dx < 0) {
        // Follow the ball.
        if (y + (HEIGHT - ball->LENGTH) / 2 < ball->predicted_y - 2)
            add_to_y(ball->speed / 8 * 5);
        else if (y + (HEIGHT - ball->LENGTH) / 2 > ball->predicted_y + 2)
            add_to_y(-(ball->speed / 8 * 5));

        // Ball is anywhere on the screen but going right.
    }
    else if (ball->dx >= 0) {
        // Left paddle slowly moves to the center.
        if (y + HEIGHT / 2 < Pong::SCREEN_HEIGHT / 2)
            add_to_y(2);
        else if (y + HEIGHT / 2 > Pong::SCREEN_HEIGHT / 2)
            add_to_y(-2);
    }
}

// Basic AI movement.
void Paddle::AI_right(Ball* ball) {
    // Ball on the left 3/5th side of the screen and going left.
    if (ball->dx >= 0) {
        // Follow the ball.
        if (y + (HEIGHT - ball->LENGTH) / 2 < ball->predicted_y - 2)
            add_to_y(ball->speed / 8 * 5);
        else if (y + (HEIGHT - ball->LENGTH) / 2 > ball->predicted_y + 2)
            add_to_y(-(ball->speed / 8 * 5));

        // Ball is anywhere on the screen but going right.
    }
    else if (ball->dx < 0) {
        // Left paddle slowly moves to the center.
        if (y + HEIGHT / 2 < Pong::SCREEN_HEIGHT / 2)
            add_to_y(2);
        else if (y + HEIGHT / 2 > Pong::SCREEN_HEIGHT / 2)
            add_to_y(-2);
    }
}

void Paddle::check_invaders()
{
    for(Invader& invader : invaders)
    {
        if (invader.x < 0) {
            invader.x = x;
            invader.y = y + HEIGHT / 2;
            invader.dx = 1;
            invader.dy = 0;
            invader.reset();
        }
        else if (invader.x > Pong::SCREEN_WIDTH) {
            invader.x = x;
            invader.y = y + HEIGHT / 2;
            invader.dx = -1;
            invader.dy = 0;
            invader.reset();
        }
        invader.step();
    }
}
