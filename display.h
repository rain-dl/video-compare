#pragma once
#include "SDL2/SDL.h"
#include <SDL2/SDL_ttf.h>
#include <array>
#include <memory>
#include <string>
#include <chrono>

struct SDL
{
    SDL();
    ~SDL();
};

class Display
{
private:
    int video_width_;
    int video_height_;
    int drawable_width_;
    int drawable_height_;
    int window_width_;
    int window_height_;
    float window_to_drawable_width_factor;
    float window_to_drawable_height_factor;
    float font_scale;

    float zoom_factor_;
    int window_center_pixel_x_;
    int window_center_pixel_y_;

    bool quit_{false};
    bool play_{true};
    bool swap_left_right_{false};
    bool show_left_{true};
    bool show_right_{true};
    bool show_hud_{true};
    bool subtraction_mode_{false};
    float seek_relative_{0.0f};
    int frame_offset_delta_{0};
    bool seek_from_start_{false};

    SDL sdl_;
    TTF_Font *small_font_;
    TTF_Font *big_font_;
    uint8_t *diff_buffer_;
    std::array<uint8_t *, 3> diff_planes_;

    SDL_Texture *left_text_texture;
    SDL_Texture *right_text_texture;
    int left_text_width;
    int left_text_height;
    int right_text_width;
    int right_text_height;

    std::chrono::milliseconds error_message_shown_at;
    SDL_Texture *error_message_texture{nullptr};
    int error_message_width;
    int error_message_height;

    SDL_Window *window_;
    SDL_Renderer *renderer_;
    SDL_Texture *texture_;

    SDL_Event event_;
    bool left_button_down_;
    bool right_button_down_;
    int right_button_down_x_;
    int right_button_down_y_;
    int window_center_pixel_anchor_x_;
    int window_center_pixel_anchor_y_;
    int mouse_x;
    int mouse_y;

private:
    void update_difference(
        std::array<uint8_t *, 3> planes_left, std::array<size_t, 3> pitches_left,
        std::array<uint8_t *, 3> planes_right, std::array<size_t, 3> pitches_right,
        int split_x);

    float get_zoom();

public:
    Display(const unsigned width, const unsigned height, const std::string &left_file_name, const std::string &right_file_name);
    ~Display();

    // Copy frame to display
    void refresh(
        std::array<uint8_t *, 3> planes_left, std::array<size_t, 3> pitches_left,
        std::array<uint8_t *, 3> planes_right, std::array<size_t, 3> pitches_right,
        const float left_position,
        const float right_position,
        const char *current_total_browsable,
        const std::string &error_message);

    // Handle events
    void input();

    bool get_quit();
    bool get_play();
    bool get_swap_left_right();
    float get_seek_relative();
    bool get_seek_from_start();
    int get_frame_offset_delta();
};
