// Copyright [2015] <Chafic Najjar>

#ifndef SRC_PONG_H_
#define SRC_PONG_H_

#include <SDL2/SDL.h>       // SDL library.
#include <SDL2/SDL_ttf.h>   // SDL font library.

#include <boost/asio.hpp>

#include <deque>
#include <iostream>
#include <string>
#include <vector>

namespace asio = boost::asio;
using boost::asio::ip::tcp;

class Ball;
class Paddle;

class Pong {
private:
    // Window and renderer.
    SDL_Window* window;     // Holds window properties.
    SDL_Renderer* renderer; // Holds rendering surface properties.

    // Game objects.
    Ball* ball;
    std::vector<Paddle*> left_paddles;
    std::vector<Paddle*> right_paddles;

    // Controllers.
    enum Controllers { mouse, keyboard, joystick };
    Controllers controller;
    SDL_Joystick* gamepad; // Holds joystick information.
    int gamepad_direction; // gamepad direction.
    int mouse_x, mouse_y;  // Mouse coordinates.

    // Fonts.

    // Font name.
    std::string font_name;

    // Font color.
    SDL_Color font_color;

    // Holds text indicating player 1 score (left).
    SDL_Texture* font_image_left_score;

    // Holds text indicating palyer 2 score (right).
    SDL_Texture* font_image_right_score;

    // Holds text indicating winner.
    SDL_Texture* font_image_winner;

    // Holds text suggesting to restart the game.
    SDL_Texture* font_image_restart;

    // Holds first part of text suggesting to launch the ball.
    SDL_Texture* font_image_launch;

    // Scores.
    int left_score;
    int right_score;

    // Indicates when rendering new score is necessary.
    bool left_score_changed;

    // Indicates when rendering new score is necessary.
    bool right_score_changed;

    // Game status.
    bool exit; // True when player wants to exit game.

    /**************************************************************************/
    // Network

    struct ClientMessage {
        float x;
        float y;
        uint8_t joy;
    } __attribute__((packed));

    struct ClientStatus {
        uint16_t points;
    };

    class Client : public std::enable_shared_from_this<Client> {
    public:
        Client(tcp::socket socket, Pong& pong);

        void start();
        void deliver(const ClientStatus& msg);

    private:
        void do_read();
        void do_write();

        tcp::socket socket_;
        Pong& pong_;
        ClientMessage read_msg_;
        std::deque<ClientStatus> write_msgs_;
        Paddle* paddle_;
    };

    class Server {
    public:
        Server(asio::io_service& io_service, const tcp::endpoint& endpoint,
               Pong& pong);

    private:
        void do_accept();

        tcp::acceptor acceptor_;
        tcp::socket socket_;
        Pong& pong_;
    };

    asio::io_service io_service_;

    Server pong_server_{
        io_service_, tcp::endpoint(tcp::v4(), /* port */ 1234), *this};

    std::vector<std::shared_ptr<Client> > left_clients;
    std::vector<std::shared_ptr<Client> > right_clients;

    /**************************************************************************/

    enum class FlutObject {
        LeftPaddle, RightPaddle, Ball, LeftInvaders, RightInvaders
    };

    class FlutClient {
    public:
        FlutClient(asio::io_service& io_service, Pong& pong,
                   FlutObject object);

        void render();

    private:
        void on_timer();
        void do_resolve();
        void do_read();

    private:
        asio::io_service& io_service_;
        bool connected_ = false;
        asio::deadline_timer timer_{io_service_, boost::posix_time::seconds(1)};
        asio::ip::tcp::resolver resolver_;
        tcp::socket socket_;
        boost::asio::streambuf read_data_;
        Pong& pong_;
        FlutObject object_;

        // big buffer to write pixel flut
        char buffer_[1024000];
    };

    std::vector<FlutClient*> flut_clients_;

public:
    // Screen resolution.
    static const int SCREEN_WIDTH;
    static const int SCREEN_HEIGHT;

    Pong(int argc, char* argv[]);
    ~Pong();
    void execute();
    void input();
    void update();
    void render();

    Paddle* add_client(std::shared_ptr<Client> client);
    void remove_client(std::shared_ptr<Client> client, Paddle* paddle);
};

#endif // SRC_PONG_H_
