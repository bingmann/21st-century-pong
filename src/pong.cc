// Copyright [2015] <Chafic Najjar>

#include "src/pong.h"

#include <iostream>
#include <thread>

#include "src/ball.h"
#include "src/paddle.h"
#include "src/utilities.h"

// human players are given only right paddles
bool players_only_on_right = true;

// Screen resolution.
const int Pong::SCREEN_WIDTH = 1920;
const int Pong::SCREEN_HEIGHT = 1080;

// PixelFlut screen offset
const int FlutX = (1920 - Pong::SCREEN_WIDTH) / 2;
const int FlutY = (1200 - Pong::SCREEN_HEIGHT) / 2;

size_t iter = 0;

class RgbColor {
public:
    unsigned char r, g, b;

    // make RGB from HSV
    static RgbColor FromHsv(unsigned char h, unsigned char s, unsigned char v);
};

RgbColor RgbColor::FromHsv(unsigned char h, unsigned char s, unsigned char v) {
    RgbColor rgb;

    if (s == 0) {
        rgb.r = v, rgb.g = v, rgb.b = v;
        return rgb;
    }

    unsigned char region = h / 43;
    unsigned char remainder = (h - (region * 43)) * 6;

    unsigned char p = (v * (255 - s)) >> 8;
    unsigned char q = (v * (255 - ((s * remainder) >> 8))) >> 8;
    unsigned char t = (v * (255 - ((s * (255 - remainder)) >> 8))) >> 8;

    switch (region) {
    case 0:
        rgb.r = v;
        rgb.g = t;
        rgb.b = p;
        break;
    case 1:
        rgb.r = q;
        rgb.g = v;
        rgb.b = p;
        break;
    case 2:
        rgb.r = p;
        rgb.g = v;
        rgb.b = t;
        break;
    case 3:
        rgb.r = p;
        rgb.g = q;
        rgb.b = v;
        break;
    case 4:
        rgb.r = t;
        rgb.g = p;
        rgb.b = v;
        break;
    default:
        rgb.r = v;
        rgb.g = p;
        rgb.b = q;
        break;
    }

    return rgb;
}

Pong::Pong(int argc, char* argv[]) {
    // Intialize SDL.
    SDL_Init(SDL_INIT_EVERYTHING);

    // Don't show cursor.
    SDL_ShowCursor(0);

    // Create window and renderer.
    window = SDL_CreateWindow("Pong",
        SDL_WINDOWPOS_UNDEFINED, // Centered window.
        SDL_WINDOWPOS_UNDEFINED, // Centered window.
        SCREEN_WIDTH, SCREEN_HEIGHT, SDL_WINDOW_SHOWN);

    renderer = SDL_CreateRenderer(
        window, -1, SDL_RENDERER_ACCELERATED | SDL_RENDERER_PRESENTVSYNC);

    // Instantiate game objects.
    ball = new Ball(SCREEN_WIDTH / 2 - ball->LENGTH / 2,
        SCREEN_HEIGHT / 2 - ball->LENGTH / 2);

    left_paddles.reserve(128);
    right_paddles.reserve(128);

    left_paddles.emplace_back(
        new Paddle(40, SCREEN_HEIGHT / 2 - Paddle::HEIGHT / 2,
                   /* right */ false));

    right_paddles.emplace_back(
        new Paddle(SCREEN_WIDTH - (40 + Paddle::WIDTH),
                   SCREEN_HEIGHT / 2 - Paddle::HEIGHT / 2,
                   /* right */ true));

    // Controllers.
    if (argc == 2) {
        if (strcmp(argv[1], "keyboard") == 0) {
            controller = keyboard;
        }
        else if (strcmp(argv[1], "joystick") == 0) {
            controller = joystick;
        }
        else {
            controller = mouse;
        }
    }
    else {
        controller = mouse; // Default controller.
    }

    if (controller == joystick) {
        printf("%i joysticks were found.\n\n", SDL_NumJoysticks());
        printf("The names of the joysticks are:\n");

        // Give control to the first joystick.
        gamepad = SDL_JoystickOpen(0);
        for (int i = 0; i < SDL_NumJoysticks(); i++) {
            std::cout << "\t" << SDL_JoystickName(gamepad) << std::endl;
        }

        // Initialize joystick controller.
        SDL_JoystickEventState(SDL_ENABLE);

        gamepad_direction = 0;
    }

    // Fonts.
    TTF_Init(); // Initialize font.
    font_color = {255, 255, 255, 255};
    font_name = "resources/fonts/NES-Chimera/NES-Chimera.ttf";
    font_image_launch =
        renderText("Press SPACE to start", font_name, font_color, 16, renderer);

    // Scores.
    left_score = 0;
    right_score = 0;

    // Indicates when rendering new score is necessary.
    left_score_changed = true;

    // Indicates when rendering new score is necessary.
    right_score_changed = true;

    // Game status.
    exit = false;

    if (0)
    {
        flut_clients_.reserve(128);

        for (size_t i = 0; i < 280; ++i) {
            flut_clients_.emplace_back(
                new FlutClient(io_service_, *this, FlutObject::LeftPaddle));
            flut_clients_.emplace_back(
                new FlutClient(io_service_, *this, FlutObject::RightPaddle));
            flut_clients_.emplace_back(
                new FlutClient(io_service_, *this, FlutObject::Ball));
        }

        for (size_t i = 0; i < 2620; ++i) {
            flut_clients_.emplace_back(
                new FlutClient(io_service_, *this, FlutObject::LeftInvaders));
            flut_clients_.emplace_back(
                new FlutClient(io_service_, *this, FlutObject::RightInvaders));
        }
    }
}

Pong::~Pong() {
    // Destroy textures.
    SDL_DestroyTexture(font_image_left_score);
    SDL_DestroyTexture(font_image_right_score);
    SDL_DestroyTexture(font_image_winner);
    SDL_DestroyTexture(font_image_restart);
    SDL_DestroyTexture(font_image_launch);

    // Close joystick.
    if (controller == joystick) {
        SDL_JoystickClose(gamepad);
    }

    // Destroy renderer and window.
    SDL_DestroyRenderer(renderer);
    SDL_DestroyWindow(window);

    // Shuts down SDL.
    SDL_Quit();
}

void Pong::execute() {

    std::thread thr1 = std::thread([this]() { io_service_.run(); });

    while (!exit) {
        input();
        update();
        render();
        SDL_Delay(10);
        iter += 4;
    }
}

void Pong::input() {

    if (ball->status == ball->READY) {
        ball->status = ball->LAUNCH;
    }

    // Handle events.
    SDL_Event event;
    while (SDL_PollEvent(&event)) {
        // Track mouse movement.
        if (event.type == SDL_MOUSEMOTION) {
            SDL_GetMouseState(&mouse_x, &mouse_y);
        }

        // Clicking 'x' or pressing F4.
        if (event.type == SDL_QUIT) {
            exit = true;
        }

        // Joystick direction controller moved.
        if (event.type == SDL_JOYAXISMOTION) {
            // 32767.
            // Up or down.
            if (event.jaxis.axis == 1) {
                gamepad_direction = event.jaxis.value / 5461;
            }
        }

        // Joystick action button pressed.
        if (event.type == SDL_JOYBUTTONDOWN) {
            if (ball->status == ball->READY) {
                ball->status = ball->LAUNCH;
            }
        }

        // Pressing a key.
        if (event.type == SDL_KEYDOWN) {
            switch (event.key.keysym.sym) {
            // Pressing ESC exits from the game.
            case SDLK_ESCAPE:
                exit = true;
                break;

            // Pressing space will launch the ball if it isn't
            // already launched.
            case SDLK_SPACE:
                if (ball->status == ball->READY) {
                    ball->status = ball->LAUNCH;
                }
                break;

            // Pressing F11 to toggle fullscreen.
            case SDLK_F11:
                int flags = SDL_GetWindowFlags(window);
                if (flags & SDL_WINDOW_FULLSCREEN) {
                    SDL_SetWindowFullscreen(window, 0);
                }
                else {
                    SDL_SetWindowFullscreen(window, SDL_WINDOW_FULLSCREEN);
                }
                break;
            }
        }
    }
}

// Update game values.
void Pong::update() {
    // Paddle movement.

    if (controller == mouse) {
        // Right paddle follows the player's mouse on the y-axis.
        // right_paddle->set_y(mouse_y);
    }
    else if (controller == joystick) {
        // Right paddle follows the player's gamepad.
        // right_paddle->add_to_y(gamepad_direction);
    }

    // AI paddle movement.
    if (left_clients.empty())
        left_paddles.front()->AI_left(ball);

    // AI paddle movement.
    if (right_clients.empty())
        right_paddles.front()->AI_right(ball);

    // Launch ball.
    if (ball->status == ball->READY) {
        return;
    }
    else if (ball->status == ball->LAUNCH) {
        ball->reset(/* socketIsFree[0] && socketIsFree[1] */ true);
        ball->launch_ball();
        if (ball->dx < 0)
            ball->predicted_y = left_paddles.front()->predict(ball);
        else
            ball->predicted_y = right_paddles.front()->predict(ball);
    }

    // Collision.
    for (Paddle* p : left_paddles) {
        if (ball->collides_with(*p)) {
            ball->bounces_off(*p);

            // Predict ball position on the y-axis.
            for (Paddle* q : right_paddles) {
                ball->predicted_y = q->predict(ball);
            }

            for (auto c : left_clients) {
                ClientStatus s { uint16_t(left_score) };
                c->deliver(s);
            }
        }
        p->check_invaders();
    }
    for (Paddle* p : right_paddles) {
        if (ball->collides_with(*p)) {
            ball->bounces_off(*p);

            // Predict ball position on the y-axis.
            for (Paddle* q : left_paddles) {
                ball->predicted_y = q->predict(ball);
            }

            for (auto c : right_clients) {
                ClientStatus s { uint16_t(right_score) };
                c->deliver(s);
            }
        }
        p->check_invaders();
    }

    // Upper and bottom walls collision.
    if (ball->wall_collision()) {
        ball->dy *= -1;                     // Reverse ball direction on y-axis.
    }

    // Update ball coordinates.
    ball->step();

    // Ball goes out.
    if (ball->x > SCREEN_WIDTH || ball->x < 0) {
        // Change score.
        if (ball->x > SCREEN_WIDTH) {
            left_score++;
            left_score_changed = true;
        }
        else {
            right_score++;
            right_score_changed = true;
        }
        ball->reset(
            /* AIonly */ left_clients.size() == 0 && right_clients.size() == 0);

        for (auto c : left_clients) {
            ClientStatus s { uint16_t(left_score) };
            c->deliver(s);
        }
        for (auto c : right_clients) {
            ClientStatus s { uint16_t(right_score) };
            c->deliver(s);
        }
    }
}

// template to draw a space invader
template <typename PutPixel>
void RenderInvader(int type, const PutPixel& put)
{
    switch (type) {
    case 0:
        put(0, 2);
        put(0, 8);
        put(1, 3);
        put(1, 7);
        put(2, 2);
        put(2, 3);
        put(2, 4);
        put(2, 5);
        put(2, 6);
        put(2, 7);
        put(2, 8);
        put(3, 1);
        put(3, 2);
        put(3, 4);
        put(3, 5);
        put(3, 6);
        put(3, 8);
        put(3, 9);
        put(4, 0);
        put(4, 1);
        put(4, 2);
        put(4, 3);
        put(4, 4);
        put(4, 5);
        put(4, 6);
        put(4, 7);
        put(4, 8);
        put(4, 9);
        put(4, 10);
        put(5, 0);
        put(5, 2);
        put(5, 3);
        put(5, 4);
        put(5, 5);
        put(5, 6);
        put(5, 7);
        put(5, 8);
        put(5, 10);
        put(6, 0);
        put(6, 2);
        put(6, 8);
        put(6, 10);
        put(7, 3);
        put(7, 4);
        put(7, 6);
        put(7, 7);
        break;

    case 1:
        put(0, 3);
        put(0, 4);
        put(1, 2);
        put(1, 3);
        put(1, 4);
        put(1, 5);
        put(2, 1);
        put(2, 2);
        put(2, 3);
        put(2, 4);
        put(2, 5);
        put(2, 6);
        put(3, 0);
        put(3, 1);
        put(3, 3);
        put(3, 4);
        put(3, 6);
        put(3, 7);
        put(4, 0);
        put(4, 1);
        put(4, 2);
        put(4, 3);
        put(4, 4);
        put(4, 5);
        put(4, 6);
        put(4, 7);
        put(5, 2);
        put(5, 5);
        put(6, 1);
        put(6, 3);
        put(6, 4);
        put(6, 6);
        put(7, 0);
        put(7, 2);
        put(7, 5);
        put(7, 7);
        break;

    case 2:
        put(0, 4);
        put(0, 5);
        put(0, 6);
        put(0, 7);
        put(1, 1);
        put(1, 2);
        put(1, 3);
        put(1, 4);
        put(1, 5);
        put(1, 6);
        put(1, 7);
        put(1, 8);
        put(1, 9);
        put(1, 10);
        put(2, 0);
        put(2, 1);
        put(2, 2);
        put(2, 3);
        put(2, 4);
        put(2, 5);
        put(2, 6);
        put(2, 7);
        put(2, 8);
        put(2, 9);
        put(2, 10);
        put(2, 11);
        put(3, 0);
        put(3, 1);
        put(3, 2);
        put(3, 5);
        put(3, 6);
        put(3, 9);
        put(3, 10);
        put(3, 11);
        put(4, 0);
        put(4, 1);
        put(4, 2);
        put(4, 3);
        put(4, 4);
        put(4, 5);
        put(4, 6);
        put(4, 7);
        put(4, 8);
        put(4, 9);
        put(4, 10);
        put(4, 11);
        put(5, 3);
        put(5, 4);
        put(5, 7);
        put(5, 8);
        put(6, 2);
        put(6, 3);
        put(6, 5);
        put(6, 6);
        put(6, 8);
        put(6, 9);
        put(7, 0);
        put(7, 1);
        put(7, 10);
        put(7, 11);
        break;

    case 3:
        put(0, 2);
        put(0, 3);
        put(0, 4);
        put(0, 5);
        put(0, 6);
        put(1, 1);
        put(1, 2);
        put(1, 3);
        put(1, 4);
        put(1, 5);
        put(1, 6);
        put(1, 7);
        put(2, 0);
        put(2, 1);
        put(2, 4);
        put(2, 7);
        put(2, 8);
        put(3, 0);
        put(3, 1);
        put(3, 2);
        put(3, 3);
        put(3, 4);
        put(3, 5);
        put(3, 6);
        put(3, 7);
        put(3, 8);
        put(4, 3);
        put(4, 4);
        put(4, 5);
        put(5, 2);
        put(5, 6);
        put(6, 1);
        put(6, 7);
        put(7, 0);
        put(7, 8);
        break;
    }
}

// Render objects on screen.
void Pong::render() {
    RgbColor rgb = RgbColor::FromHsv((iter / 1) % 256, 255, 255);

    // Clear screen (background color).
    SDL_SetRenderDrawColor(renderer, 0, 0, 0, 255); // Dark grey.
    SDL_RenderClear(renderer);

    // Paddle color.
    SDL_SetRenderDrawColor(renderer, rgb.r, rgb.g, rgb.b, 255);

    // Render filled paddle.
    for (Paddle* p : left_paddles) {
        SDL_Rect paddle1 = {p->get_x(), p->get_y(),
                            Paddle::WIDTH, Paddle::HEIGHT};
        SDL_RenderFillRect(renderer, &paddle1);

        // Render invaders
        for (Invader& invader : p->invaders) {
            RenderInvader(
                invader.type,
                [&](unsigned x, unsigned y) {
                    unsigned sx = 5, sy = 5;
                    for (size_t i = 0; i < sx; ++i) {
                        for (size_t j = 0; j < sy; ++j) {
                            SDL_RenderDrawPoint(
                                renderer,
                                invader.x + sx * (8 - x) + i,
                                invader.y + sy * y + j);
                        }
                    }
                });
        }
    }

    // Render filled paddle.
    for (Paddle* p : right_paddles) {
        SDL_Rect paddle2 = {p->get_x(), p->get_y(),
                            Paddle::WIDTH, Paddle::HEIGHT};
        SDL_RenderFillRect(renderer, &paddle2);

        // Render invaders
        for (Invader& invader : p->invaders) {
            RenderInvader(
                invader.type,
                [&](unsigned x, unsigned y) {
                    unsigned sx = 5, sy = 5;
                    for (size_t i = 0; i < sx; ++i) {
                        for (size_t j = 0; j < sy; ++j) {
                            SDL_RenderDrawPoint(
                                renderer,
                                invader.x + sx * x + i,
                                invader.y + sy * y + j);
                        }
                    }
                });
        }
    }

    // Render ball.
    SDL_Rect pong_ball = {ball->x, ball->y, ball->LENGTH, ball->LENGTH};
    SDL_RenderFillRect(renderer, &pong_ball);

    // Render scores.
    if (left_score_changed) {
        font_image_left_score = renderText(
            std::to_string(left_score), font_name, font_color, 24, renderer);
        left_score_changed = false;
        if (left_score == 5) {
            font_image_winner = renderText(
                "Player 1 won!", font_name, font_color, 24, renderer);
            font_image_restart = renderText(
                "Press SPACE to restart", font_name, font_color, 14, renderer);
        }
    }
    renderTexture(font_image_left_score, renderer, SCREEN_WIDTH * 4 / 10,
        SCREEN_HEIGHT / 12);

    int score_font_size = 24;
    if (right_score_changed) {
        font_image_right_score = renderText(std::to_string(right_score),
            font_name, font_color, score_font_size, renderer);
        right_score_changed = false;
        if (right_score == 5) {
            font_image_winner = renderText(
                "Player 2 won!", font_name, font_color, 24, renderer);
            font_image_restart = renderText(
                "Press SPACE to restart", font_name, font_color, 14, renderer);
        }
    }
    renderTexture(font_image_right_score, renderer,
        SCREEN_WIDTH * 6 / 10 - score_font_size / 2, SCREEN_HEIGHT / 12);

    // Render text indicating the winner.
    if (left_score == 5) {
        // Align with score.
        renderTexture(font_image_winner, renderer, SCREEN_WIDTH * 1 / 10 + 3,
            SCREEN_HEIGHT / 4);
        renderTexture(font_image_restart, renderer, SCREEN_WIDTH * 1 / 10 + 3,
            SCREEN_HEIGHT / 3);
        if (ball->status == ball->LAUNCHED) {
            left_score = 0;
            right_score = 0;
            left_score_changed = true;
            right_score_changed = true;
        }
    }
    else if (right_score == 5) {
        // Align with score.
        renderTexture(font_image_winner, renderer,
            SCREEN_WIDTH * 6 / 10 - score_font_size / 2, SCREEN_HEIGHT / 4);
        renderTexture(font_image_restart, renderer,
            SCREEN_WIDTH * 6 / 10 - score_font_size / 2, SCREEN_HEIGHT / 3);
        if (ball->status == ball->LAUNCHED) {
            left_score = 0;
            right_score = 0;
            left_score_changed = true;
            right_score_changed = true;
        }
    }
    else if (ball->status == ball->READY) {
        // Draw "Press SPACE to start".
        renderTexture(font_image_launch, renderer, SCREEN_WIDTH / 2 - 160,
            SCREEN_HEIGHT - 30);
    }

    // Swap buffers.
    SDL_RenderPresent(renderer);
}

Paddle* Pong::add_client(std::shared_ptr<Client> client)
{
    if (left_clients.size() < right_clients.size() && !players_only_on_right) {
        left_clients.emplace_back(client);
        if (left_paddles.size() < left_clients.size()) {
            left_paddles.emplace_back(
                new Paddle(20 + 20 * left_paddles.size(),
                           SCREEN_HEIGHT / 2 - Paddle::HEIGHT / 2,
                           /* right */ false));
        }
        return left_paddles[left_clients.size() - 1];
    }
    else {
        right_clients.emplace_back(client);
        if (right_paddles.size() < right_clients.size()) {
            right_paddles.emplace_back(
                new Paddle(20 + 20 * right_paddles.size(),
                           SCREEN_HEIGHT / 2 - Paddle::HEIGHT / 2,
                           /* right */ true));
        }
        return right_paddles[right_clients.size() - 1];
    }
}

//! remove all items from vector matching val, returns number of items deleted.
template <typename V, typename T, typename Equal = std::equal_to<T> >
size_t vector_erase(std::vector<V>& vec, const T& val,
                    const Equal& equal = Equal())
{
    typename std::vector<V>::iterator it = std::remove_if(
        vec.begin(), vec.end(),
        [&val, &equal](const V& a) { return equal(a, val); });
    size_t size = vec.end() - it;
    vec.erase(it, vec.end());
    return size;
}

void Pong::remove_client(std::shared_ptr<Client> client, Paddle* paddle)
{
    vector_erase(left_clients, client);
    if (left_paddles.size() > 1)
        vector_erase(left_paddles, paddle);

    vector_erase(right_clients, client);
    if (right_paddles.size() > 1)
        vector_erase(right_paddles, paddle);
}

/******************************************************************************/
// Pong::Client

Pong::Client::Client(tcp::socket socket, Pong& pong)
    : socket_(std::move(socket)), pong_(pong) { }

void Pong::Client::start()
{
    paddle_ = pong_.add_client(shared_from_this());
    do_read();
}

void Pong::Client::deliver(const ClientStatus& msg)
{
    bool write_in_progress = !write_msgs_.empty();
    write_msgs_.push_back(msg);
    if (!write_in_progress)
        do_write();
}

void Pong::Client::do_read() {
    auto self(shared_from_this());
    asio::async_read(
        socket_,
        asio::buffer(&read_msg_, sizeof(read_msg_)),
        [this, self](boost::system::error_code ec, std::size_t) {
            if (ec) {
                std::cout << "Client::do_read() removing client" << std::endl;
                pong_.remove_client(shared_from_this(), paddle_);
                socket_.close();
            }
            else {
                std::cout << "y = " << read_msg_.y << std::endl;
                if (paddle_) {
                    if (paddle_->right)
                        paddle_->set_x(
                            SCREEN_WIDTH * 3/5 - 10 +
                            read_msg_.x / 128.0 * SCREEN_WIDTH * 2/5);
                    else
                        paddle_->set_x(read_msg_.x / 128.0 * SCREEN_WIDTH * 2/5 + 10);

                    paddle_->set_y(read_msg_.y / 128.0 * SCREEN_HEIGHT);
                }
                do_read();
            }
        });
}

void Pong::Client::do_write() {
    auto self(shared_from_this());
    asio::async_write(
        socket_,
        asio::buffer(&write_msgs_.front(), sizeof(write_msgs_.front())),
        [this, self](boost::system::error_code ec, std::size_t) {
            if (!ec) {
                write_msgs_.pop_front();
                if (!write_msgs_.empty())
                    do_write();
            }
        });
}

/******************************************************************************/
// Pong::Server

Pong::Server::Server(
    asio::io_service& io_service, const tcp::endpoint& endpoint,
    Pong& pong)
    : acceptor_(io_service, endpoint), socket_(io_service),
      pong_(pong)
{
    do_accept();
}

void Pong::Server::do_accept()
{
    acceptor_.async_accept(
        socket_,
        [this](boost::system::error_code ec) {
            if (!ec) {
                std::cout << "Server: new client socket" << std::endl;
                std::make_shared<Client>(
                    std::move(socket_), pong_)->start();
            }
            do_accept();
        });
}

/******************************************************************************/
// Pong::FlutClient

void renderPixel(char* output, size_t* pos, size_t size, size_t x, size_t y,
                 size_t color) {
    *pos += snprintf(
        output + *pos, size - *pos,
        "PX %u %u %06x\n",
        unsigned(FlutX + x), unsigned(FlutY + y), unsigned(color));
}

void renderRect(char* output, size_t* pos, size_t size, size_t x, size_t y,
    size_t width, size_t height, size_t color) {
    for (size_t i = FlutX + x; i < FlutX + x + width; ++i) {
        for (size_t j = FlutY + y; j < FlutY + y + height; ++j) {
            *pos += snprintf(
                output + *pos, size - *pos,
                "PX %u %u %06x\n", unsigned(i), unsigned(j), unsigned(color));
        }
    }
}

void renderRectRound(
    char* output, size_t* pos, size_t size, size_t x, size_t y,
    size_t width, size_t height, size_t color) {
    for (size_t i = 0; i < width; ++i) {
        for (size_t j = 0; j < height; ++j) {
            // if (i + j > height / 3) continue;
            *pos += snprintf(
                output + *pos, size - *pos, "PX %u %u %06x\n",
                unsigned(FlutX + x + i), unsigned(FlutY + y + j),
                unsigned(color));
        }
    }
}

Pong::FlutClient::FlutClient(asio::io_service& io_service, Pong& pong,
                             FlutObject object)
    : io_service_(io_service), resolver_(io_service),
      socket_(io_service), pong_(pong), object_(object) {
    timer_.async_wait(
        [this](const boost::system::error_code&) { on_timer(); });
}

void Pong::FlutClient::on_timer() {
    if (!connected_)
        do_resolve();

    timer_.expires_from_now(boost::posix_time::seconds(1));
    timer_.async_wait(
        [&](const boost::system::error_code&) { on_timer(); });
}

void Pong::FlutClient::do_resolve() {
    resolver_.async_resolve(
        asio::ip::tcp::resolver::query(
            asio::ip::tcp::v4(), "94.45.231.39", /* port */ "1234"),
        [this](const boost::system::error_code& ec,
               const asio::ip::tcp::resolver::iterator& iterator) {
            if (!ec) {
                asio::async_connect(
                    socket_, iterator,
                    [this](const boost::system::error_code& ec,
                           const asio::ip::tcp::resolver::iterator&) {
                        if (!ec) {
                            connected_ = true;
                            std::cout << "FlutClient::connect() success"
                                      << std::endl;

                            // boost::asio::socket_base::send_buffer_size option(64 * 8192);
                            // socket_.set_option(option);

                            do_read();
                            render();
                        }
                        else {
                            std::cout << "FlutClient::do_connect() "
                                      << ec << std::endl;
                        }
                    });
            }
            else {
                std::cout << "FlutClient::do_resolve() error "
                          << ec << std::endl;
            }
        });
}

void Pong::FlutClient::do_read() {
    asio::async_read_until(socket_, read_data_, "\r\n",
                           [this](boost::system::error_code ec, std::size_t) {
                               if (!ec) {
                                   std::istream is(&read_data_);
                                   std::cout << "FlutClient <<< " << is.rdbuf()
                                             << std::endl;
                                   do_read();
                               }
                               else {
                                   socket_.close();
                                   connected_ = false;
                               }
                           });
}

void Pong::FlutClient::render() {
    RgbColor rgb = RgbColor::FromHsv((iter / 1) % 256, 255, 255);
    uint32_t color = (rgb.r << 16) + (rgb.g << 8) + rgb.b;

    size_t buffer_pos = 0;

    if (object_ == FlutObject::LeftPaddle)
    {
        for (Paddle* p : pong_.left_paddles) {
            renderRect(buffer_, &buffer_pos, sizeof(buffer_),
                       p->get_x(), p->get_y(), Paddle::WIDTH,
                       Paddle::HEIGHT, color);
        }
    }

    if (object_ == FlutObject::RightPaddle)
    {
        for (Paddle* p : pong_.right_paddles) {
            renderRect(buffer_, &buffer_pos, sizeof(buffer_),
                       p->get_x(), p->get_y(), Paddle::WIDTH,
                       Paddle::HEIGHT, color);
        }
    }

    if (object_ == FlutObject::Ball)
    {
        renderRectRound(
            buffer_, &buffer_pos, sizeof(buffer_), pong_.ball->x,
            pong_.ball->y, pong_.ball->LENGTH, pong_.ball->LENGTH, color);
    }

    size_t x = 0;

    if (object_ == FlutObject::LeftInvaders)
    {
        for (Paddle* p : pong_.left_paddles) {
            for (Invader& invader : p->invaders) {

                RgbColor rgb = RgbColor::FromHsv((iter + (x += 78)) % 256, 255, 255);
                uint32_t color = (rgb.r << 16) + (rgb.g << 8) + rgb.b;

                // Render invaders
                RenderInvader(
                    invader.type,
                    [&](unsigned x, unsigned y) {
                        unsigned sx = 5, sy = 5;
                        for (size_t i = 0; i < sx; ++i) {
                            for (size_t j = 0; j < sy; ++j) {
                                renderPixel(
                                    buffer_, &buffer_pos, sizeof(buffer_),
                                    invader.x + sx * (8 - x) + i,
                                    invader.y + sy * y + j,
                                    color);
                            }
                        }
                    });
            }
        }
    }

    if (object_ == FlutObject::RightInvaders)
    {
        for (Paddle* p : pong_.right_paddles) {
            for (Invader& invader : p->invaders) {

                RgbColor rgb = RgbColor::FromHsv((iter + (x += 78)) % 256, 255, 255);
                uint32_t color = (rgb.r << 16) + (rgb.g << 8) + rgb.b;

                // Render invaders
                RenderInvader(
                    invader.type,
                    [&](unsigned x, unsigned y) {
                        unsigned sx = 5, sy = 5;
                        for (size_t i = 0; i < sx; ++i) {
                            for (size_t j = 0; j < sy; ++j) {
                                renderPixel(
                                    buffer_, &buffer_pos, sizeof(buffer_),
                                    invader.x + sx * x + i,
                                    invader.y + sy * y + j,
                                    color);
                            }
                        }
                    });
            }
        }
    }

    asio::async_write(socket_, asio::buffer(buffer_, buffer_pos),
        [this](boost::system::error_code ec, std::size_t /*length*/) {
            if (!ec) {
                render();
            }
            else {
                std::cout << "FlutClient: connected closed" << std::endl;
                socket_.close();
                connected_ = false;
            }
        });
}


