#include "display.h"
#include <stdexcept>
#include <string>
#include <sstream>
#include <iostream>
#include <memory>
#include <cmath>
#include <tuple>

template <typename T>
inline T check_SDL(T value, const std::string& message)
{
	if (!value)
	{
		throw std::runtime_error{ "SDL " + message };
	}
	else
	{
		return value;
	}
}

inline int clampIntToByteRange(int value)
{
	return value > 255 ? 255 : value < 0 ? 0
		: value;
}

inline uint8_t clampIntToByte(int value)
{
	return (uint8_t)clampIntToByteRange(value);
}

static const SDL_Color textColor = { 255, 255, 255, 0 };

SDL::SDL()
{
	check_SDL(!SDL_Init(SDL_INIT_VIDEO | SDL_INIT_TIMER), "SDL init");
	check_SDL(!TTF_Init(), "TTF init");
}

SDL::~SDL()
{
	SDL_Quit();
}

Display::Display(
	const unsigned width,
	const unsigned height,
	const std::string& left_file_name,
	const std::string& right_file_name) :
	video_width_{ (int)width },
	video_height_{ (int)height }
{
	zoom_factor_ = 0.0;
	window_center_pixel_x_ = video_width_ / 2;
	window_center_pixel_y_ = video_height_ / 2;

	SDL_DisplayMode DM;
	SDL_GetCurrentDisplayMode(0, &DM);
	int window_width = DM.w;
	int window_height = DM.h;

#ifdef _DEBUG
	const int create_window_flags = SDL_WINDOW_SHOWN | SDL_WINDOW_MAXIMIZED;
#else
	const int create_window_flags = SDL_WINDOW_SHOWN | SDL_WINDOW_FULLSCREEN | SDL_WINDOW_BORDERLESS;
#endif

	window_ = check_SDL(SDL_CreateWindow(
		"video-compare", SDL_WINDOWPOS_UNDEFINED, SDL_WINDOWPOS_UNDEFINED,
		window_width, window_height, create_window_flags),
		"window");

	renderer_ = check_SDL(SDL_CreateRenderer(
		window_, -1,
		SDL_RENDERER_ACCELERATED | SDL_RENDERER_PRESENTVSYNC),
		"renderer");

	SDL_SetHint(SDL_HINT_RENDER_SCALE_QUALITY, "nearest");

	SDL_SetRenderDrawColor(renderer_, 0, 0, 0, 255);
	SDL_RenderClear(renderer_);
	SDL_RenderPresent(renderer_);

	SDL_GL_GetDrawableSize(window_, &drawable_width_, &drawable_height_);
	SDL_GetWindowSize(window_, &window_width_, &window_height_);

	window_to_drawable_width_factor = (float)drawable_width_ / (float)window_width_;
	window_to_drawable_height_factor = (float)drawable_height_ / (float)window_height_;
	font_scale = (window_to_drawable_width_factor + window_to_drawable_height_factor) / 2.0f;

	std::stringstream ss;
	ss << SDL_GetBasePath() << "SourceCodePro-Regular.ttf";
	std::string font_filename = ss.str();

	small_font_ = check_SDL(TTF_OpenFont(font_filename.c_str(), 16 * font_scale), "font open");
	big_font_ = check_SDL(TTF_OpenFont(font_filename.c_str(), 24 * font_scale), "font open");

	SDL_RenderSetLogicalSize(renderer_, drawable_width_, drawable_height_);
	texture_ = check_SDL(SDL_CreateTexture(
		renderer_, SDL_PIXELFORMAT_RGB24, SDL_TEXTUREACCESS_STREAMING,
		width, height),
		"renderer");

	SDL_Surface* textSurface = TTF_RenderText_Blended(small_font_, left_file_name.c_str(), textColor);
	left_text_texture = SDL_CreateTextureFromSurface(renderer_, textSurface);
	left_text_width = textSurface->w;
	left_text_height = textSurface->h;
	SDL_FreeSurface(textSurface);

	textSurface = TTF_RenderText_Blended(small_font_, right_file_name.c_str(), textColor);
	right_text_texture = SDL_CreateTextureFromSurface(renderer_, textSurface);
	right_text_width = textSurface->w;
	right_text_height = textSurface->h;
	SDL_FreeSurface(textSurface);

	diff_buffer_ = new uint8_t[video_width_ * video_height_ * 3];
	uint8_t* diff_plane_0 = diff_buffer_;

	diff_planes_ = { diff_plane_0, NULL, NULL };

	mouse_x = window_width_ / 2;
	mouse_y = window_height_ / 2;
	left_button_down_ = false;
	right_button_down_ = false;
}

Display::~Display()
{
	SDL_DestroyTexture(texture_);
	SDL_DestroyTexture(left_text_texture);
	SDL_DestroyTexture(right_text_texture);

	if (error_message_texture != nullptr)
	{
		SDL_DestroyTexture(error_message_texture);
	}

	TTF_CloseFont(small_font_);
	TTF_CloseFont(big_font_);

	delete[] diff_buffer_;

	SDL_DestroyRenderer(renderer_);
	SDL_DestroyWindow(window_);
}

void Display::update_difference(
	std::array<uint8_t*, 3> planes_left, std::array<size_t, 3> pitches_left,
	std::array<uint8_t*, 3> planes_right, std::array<size_t, 3> pitches_right,
	int split_x)
{
	uint8_t* p_left_r = planes_left[0] + split_x * 3;
	uint8_t* p_left_g = planes_left[0] + split_x * 3 + 1;
	uint8_t* p_left_b = planes_left[0] + split_x * 3 + 2;

	uint8_t* p_right_r = planes_right[0] + split_x * 3;
	uint8_t* p_right_g = planes_right[0] + split_x * 3 + 1;
	uint8_t* p_right_b = planes_right[0] + split_x * 3 + 2;

	uint8_t* p_diff_r = diff_planes_[0] + split_x * 3;
	uint8_t* p_diff_g = diff_planes_[0] + split_x * 3 + 1;
	uint8_t* p_diff_b = diff_planes_[0] + split_x * 3 + 2;

	const int amplification = 2;

	for (int y = 0; y < video_height_; y++)
	{
		for (int x = 0; x < (video_width_ - split_x); x++)
		{
			int rgb1[3], rgb2[3], diff[3];

			rgb1[0] = p_left_r[x * 3];
			rgb1[1] = p_left_g[x * 3];
			rgb1[2] = p_left_b[x * 3];

			rgb2[0] = p_right_r[x * 3];
			rgb2[1] = p_right_g[x * 3];
			rgb2[2] = p_right_b[x * 3];

			diff[0] = abs(rgb1[0] - rgb2[0]) * amplification;
			diff[1] = abs(rgb1[1] - rgb2[1]) * amplification;
			diff[2] = abs(rgb1[2] - rgb2[2]) * amplification;

			p_diff_r[x * 3] = clampIntToByte(diff[0]);
			p_diff_g[x * 3] = clampIntToByte(diff[1]);
			p_diff_b[x * 3] = clampIntToByte(diff[2]);
		}

		p_left_r += pitches_left[0];
		p_left_g += pitches_left[0];
		p_left_b += pitches_left[0];

		p_right_r += pitches_right[0];
		p_right_g += pitches_right[0];
		p_right_b += pitches_right[0];

		p_diff_r += video_width_ * 3;
		p_diff_g += video_width_ * 3;
		p_diff_b += video_width_ * 3;
	}
}

float Display::get_zoom()
{
	if (zoom_factor_ >= 0)
		return 1 + zoom_factor_;
	else
		return 1 / (1 - zoom_factor_);
}

void Display::refresh(
	std::array<uint8_t*, 3> planes_left, std::array<size_t, 3> pitches_left,
	std::array<uint8_t*, 3> planes_right, std::array<size_t, 3> pitches_right,
	const float left_position,
	const float right_position,
	const char* current_total_browsable,
	const std::string& error_message)
{
	bool compare_mode = show_left_ && show_right_;
	float zoom = get_zoom();

	int mouse_video_x = std::min(std::max(0, (int)std::round((mouse_x - window_width_ / 2) / zoom) + window_center_pixel_x_), video_width_);
	int mouse_video_y = std::min(std::max(0, (int)std::round((mouse_y - window_height_ / 2) / zoom) + window_center_pixel_y_), video_height_);

	// clear everything
	SDL_RenderClear(renderer_);

	if (show_left_ || show_right_)
	{
		int split_x = compare_mode ? mouse_video_x : show_left_ ? video_width_ : 0;

		// update video
		if (show_left_ && (split_x > 0))
		{
			SDL_Rect render_quad_left = { 0, 0, split_x, video_height_ };

			check_SDL(!SDL_UpdateTexture(texture_, &render_quad_left, planes_left[0], pitches_left[0]), "left texture update (video mode)");
		}
		if (show_right_ && (split_x < (video_width_ - 1)))
		{
			SDL_Rect render_quad_right = { split_x, 0, (video_width_ - split_x), video_height_ };

			if (subtraction_mode_)
			{
				update_difference(planes_left, pitches_left, planes_right, pitches_right, split_x);

				check_SDL(!SDL_UpdateTexture(
					texture_, &render_quad_right,
					diff_planes_[0] + split_x * 3, video_width_ * 3),
					"right texture update (subtraction mode)");
			}
			else
			{
				check_SDL(!SDL_UpdateTexture(texture_, &render_quad_right, planes_right[0] + split_x * 3, pitches_right[0]), "right texture update (video mode)");
			}
		}

		// render video
		int src_x_offset = std::min(std::max(0, (int)(window_center_pixel_x_ - window_width_ / zoom / 2)), video_width_);
		int src_y_offset = std::min(std::max(0, (int)(window_center_pixel_y_ - window_height_ / zoom / 2)), video_height_);
		SDL_Rect src_zoomed_area = { src_x_offset, src_y_offset,
			std::min((int)(window_center_pixel_x_ + window_width_ / zoom / 2), video_width_) - src_x_offset,
			std::min((int)(window_center_pixel_y_ + window_height_ / zoom / 2), video_height_) - src_y_offset };
		SDL_Rect dst_zoomed_area = { std::min(std::max(0, (int)(window_width_ / 2 - (window_center_pixel_x_ - src_zoomed_area.x) * zoom)), window_width_),
			std::min(std::max(0, (int)(window_height_ / 2 - (window_center_pixel_y_ - src_zoomed_area.y) * zoom)), window_height_),
			std::min((int)(src_zoomed_area.w * zoom), window_width_), std::min((int)(src_zoomed_area.h * zoom), window_height_) };
		SDL_RenderCopy(renderer_, texture_, &src_zoomed_area, &dst_zoomed_area);
	}

	SDL_Rect fill_rect;
	SDL_Rect text_rect;
	SDL_Surface* textSurface;

	if (show_hud_)
	{
		// render background rectangles and text on top
		char buffer[20];
		int border_extension = 3 * font_scale;
		int border_extension_x2 = border_extension * 2;
		int line1_y = 20;
		int line2_y = line1_y + 30 * font_scale;

		SDL_SetRenderDrawColor(renderer_, 0, 0, 0, 64);
		SDL_SetRenderDrawBlendMode(renderer_, SDL_BLENDMODE_BLEND);
		if (show_left_)
		{
			// file name and current position of left video
			sprintf(buffer, "%.2f", left_position);
			textSurface = TTF_RenderText_Blended(small_font_, buffer, textColor);
			SDL_Texture* left_position_text_texture = SDL_CreateTextureFromSurface(renderer_, textSurface);
			int left_position_text_width = textSurface->w;
			int left_position_text_height = textSurface->h;
			SDL_FreeSurface(textSurface);

			fill_rect = { line1_y - border_extension, line1_y - border_extension, left_text_width + border_extension_x2, left_text_height + border_extension_x2 };
			SDL_RenderFillRect(renderer_, &fill_rect);
			fill_rect = { line1_y - border_extension, line2_y - border_extension, left_position_text_width + border_extension_x2, left_position_text_height + border_extension_x2 };
			SDL_RenderFillRect(renderer_, &fill_rect);
			text_rect = { line1_y, line1_y, left_text_width, left_text_height };
			SDL_RenderCopy(renderer_, left_text_texture, NULL, &text_rect);
			text_rect = { line1_y, line2_y, left_position_text_width, left_position_text_height };
			SDL_RenderCopy(renderer_, left_position_text_texture, NULL, &text_rect);
			SDL_DestroyTexture(left_position_text_texture);
		}
		if (show_right_)
		{
			// file name and current position of right video
			sprintf(buffer, "%.2f", right_position);
			textSurface = TTF_RenderText_Blended(small_font_, buffer, textColor);
			SDL_Texture* right_position_text_texture = SDL_CreateTextureFromSurface(renderer_, textSurface);
			int right_position_text_width = textSurface->w;
			int right_position_text_height = textSurface->h;
			SDL_FreeSurface(textSurface);

			fill_rect = { drawable_width_ - line1_y - border_extension - right_text_width, line1_y - border_extension, right_text_width + border_extension_x2, right_text_height + border_extension_x2 };
			SDL_RenderFillRect(renderer_, &fill_rect);
			fill_rect = { drawable_width_ - line1_y - border_extension - right_position_text_width, line2_y - border_extension, right_position_text_width + border_extension_x2, right_position_text_height + border_extension_x2 };
			SDL_RenderFillRect(renderer_, &fill_rect);
			text_rect = { drawable_width_ - line1_y - right_text_width, line1_y, right_text_width, right_text_height };
			SDL_RenderCopy(renderer_, right_text_texture, NULL, &text_rect);
			text_rect = { drawable_width_ - line1_y - right_position_text_width, line2_y, right_position_text_width, right_position_text_height };
			SDL_RenderCopy(renderer_, right_position_text_texture, NULL, &text_rect);
			SDL_DestroyTexture(right_position_text_texture);
		}

		char center_text[128];
		sprintf(center_text, "%s  Zoom: %.2f", current_total_browsable, zoom);

		// current frame / no. in history buffer
		textSurface = TTF_RenderText_Blended(small_font_, center_text, textColor);
		SDL_Texture* current_total_browsable_text_texture = SDL_CreateTextureFromSurface(renderer_, textSurface);
		int current_total_browsable_text_width = textSurface->w;
		int current_total_browsable_text_height = textSurface->h;
		SDL_FreeSurface(textSurface);

		fill_rect = { drawable_width_ / 2 - current_total_browsable_text_width / 2 - border_extension, line1_y - border_extension, current_total_browsable_text_width + border_extension_x2, current_total_browsable_text_height + border_extension_x2 };
		SDL_RenderFillRect(renderer_, &fill_rect);
		text_rect = { drawable_width_ / 2 - current_total_browsable_text_width / 2, line1_y, current_total_browsable_text_width, current_total_browsable_text_height };
		SDL_RenderCopy(renderer_, current_total_browsable_text_texture, NULL, &text_rect);
		SDL_DestroyTexture(current_total_browsable_text_texture);
	}

	// render (optional) error message
	if (!error_message.empty())
	{
		error_message_shown_at = std::chrono::duration_cast<std::chrono::milliseconds>(std::chrono::system_clock::now().time_since_epoch());
		textSurface = TTF_RenderText_Blended(big_font_, error_message.c_str(), textColor);
		error_message_texture = SDL_CreateTextureFromSurface(renderer_, textSurface);
		error_message_width = textSurface->w;
		error_message_height = textSurface->h;
		SDL_FreeSurface(textSurface);
	}
	if (error_message_texture != nullptr)
	{
		std::chrono::milliseconds now = std::chrono::duration_cast<std::chrono::milliseconds>(std::chrono::system_clock::now().time_since_epoch());
		float keep_alpha = std::max(sqrt(1.0f - (now - error_message_shown_at).count() / 1000.0f / 4.0f), 0.0f);

		SDL_SetRenderDrawColor(renderer_, 0, 0, 0, 64 * keep_alpha);
		fill_rect = { drawable_width_ / 2 - error_message_width / 2 - 2, drawable_height_ / 2 - error_message_height / 2 - 2, error_message_width + 4, error_message_height + 4 };
		SDL_RenderFillRect(renderer_, &fill_rect);

		SDL_SetTextureAlphaMod(error_message_texture, 255 * keep_alpha);
		text_rect = { drawable_width_ / 2 - error_message_width / 2, drawable_height_ / 2 - error_message_height / 2, error_message_width, error_message_height };
		SDL_RenderCopy(renderer_, error_message_texture, NULL, &text_rect);
	}

	if (show_hud_ && compare_mode)
	{
		int draw_x = std::round(float(mouse_x) * window_to_drawable_width_factor);
		int draw_y = std::round(float(mouse_y) * window_to_drawable_width_factor);

		// render movable slider(s)
		SDL_SetRenderDrawColor(renderer_, 255, 255, 255, SDL_ALPHA_OPAQUE);
		SDL_RenderDrawLine(renderer_, draw_x, 0, draw_x, drawable_height_);
	}

	SDL_RenderPresent(renderer_);
}

void Display::input()
{
	if (left_button_down_)
		SDL_GetMouseState(&mouse_x, &mouse_y);

	if (right_button_down_)
	{
		int x, y;
		SDL_GetMouseState(&x, &y);
		window_center_pixel_x_ = window_center_pixel_anchor_x_ + (right_button_down_x_ - x) / get_zoom();
		window_center_pixel_y_ = window_center_pixel_anchor_y_ + (right_button_down_y_ - y) / get_zoom();
	}

	seek_relative_ = 0.0f;
	seek_from_start_ = false;
	frame_offset_delta_ = 0;

	while (SDL_PollEvent(&event_))
	{
		switch (event_.type)
		{
		case SDL_MOUSEBUTTONDOWN:
			if (event_.button.button == SDL_BUTTON_LEFT)
			{
				auto mod = SDL_GetModState();
				if ((mod & KMOD_LCTRL) != 0 || (mod & KMOD_RCTRL) != 0)
				{
					int x, y;
					SDL_GetMouseState(&x, &y);
					seek_relative_ = float(x) / float(window_width_);
					seek_from_start_ = true;
				}
				else
				{
					left_button_down_ = true;
					SDL_GetMouseState(&mouse_x, &mouse_y);
				}
			}
			if (event_.button.button == SDL_BUTTON_RIGHT)
			{
				right_button_down_ = true;
				SDL_GetMouseState(&right_button_down_x_, &right_button_down_y_);
				window_center_pixel_anchor_x_ = window_center_pixel_x_;
				window_center_pixel_anchor_y_ = window_center_pixel_y_;
			}
			break;
		case SDL_MOUSEBUTTONUP:
			if (event_.button.button == SDL_BUTTON_LEFT)
				left_button_down_ = false;
			if (event_.button.button == SDL_BUTTON_RIGHT)
				right_button_down_ = false;
			break;
		case SDL_MOUSEWHEEL:
			if (event_.wheel.y > 0)
				zoom_factor_ += 0.2;
			else if (event_.wheel.y < 0)
				zoom_factor_ -= 0.2;
			break;
		case SDL_KEYDOWN:
			switch (event_.key.keysym.sym)
			{
			case SDLK_ESCAPE:
				quit_ = true;
				break;
			case SDLK_SPACE:
				play_ = !play_;
				break;
			case SDLK_1:
				show_left_ = !show_left_;
				break;
			case SDLK_2:
				show_right_ = !show_right_;
				break;
			case SDLK_3:
				show_hud_ = !show_hud_;
				break;
			case SDLK_0:
				subtraction_mode_ = !subtraction_mode_;
				break;
			case SDLK_a:
				frame_offset_delta_ += 1;
				break;
			case SDLK_d:
				frame_offset_delta_ -= 1;
				break;
			case SDLK_s:
			{
				swap_left_right_ = !swap_left_right_;

				SDL_Texture* temp = left_text_texture;
				left_text_texture = right_text_texture;
				right_text_texture = temp;

				int temp_dim = left_text_width;
				left_text_width = right_text_width;
				right_text_width = temp_dim;

				temp_dim = left_text_height;
				left_text_height = right_text_height;
				right_text_height = temp_dim;
				break;
			}
			case SDLK_LEFT:
				seek_relative_ -= 1.0f;
				break;
			case SDLK_DOWN:
				seek_relative_ -= 10.0f;
				break;
			case SDLK_PAGEDOWN:
				seek_relative_ -= 600.0f;
				break;
			case SDLK_RIGHT:
				seek_relative_ += 1.0f;
				break;
			case SDLK_UP:
				seek_relative_ += 10.0f;
				break;
			case SDLK_PAGEUP:
				seek_relative_ += 600.0f;
				break;
			case SDLK_PLUS:
			case SDLK_KP_PLUS:
				zoom_factor_ += 0.2;
				break;
			case SDLK_MINUS:
			case SDLK_KP_MINUS:
				zoom_factor_ -= 0.2;
				break;
			case SDLK_KP_4:
				window_center_pixel_x_ -= window_width_ / get_zoom() / 20;
				break;
			case SDLK_KP_6:
				window_center_pixel_x_ += window_width_ / get_zoom() / 20;
				break;
			case SDLK_KP_8:
				window_center_pixel_y_ -= window_height_ / get_zoom() / 20;
				break;
			case SDLK_KP_2:
				window_center_pixel_y_ += window_height_ / get_zoom() / 20;
				break;
			case SDLK_KP_5:
				window_center_pixel_x_ = video_width_ / 2;
				window_center_pixel_y_ = video_height_ / 2;
				break;
			default:
				break;
			}
			break;
		case SDL_QUIT:
			quit_ = true;
			break;
		default:
			break;
		}
	}
}

bool Display::get_quit()
{
	return quit_;
}

bool Display::get_play()
{
	return play_;
}

bool Display::get_swap_left_right()
{
	return swap_left_right_;
}

float Display::get_seek_relative()
{
	return seek_relative_;
}

bool Display::get_seek_from_start()
{
	return seek_from_start_;
}

int Display::get_frame_offset_delta()
{
	return frame_offset_delta_;
}
