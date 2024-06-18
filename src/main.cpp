#include <algorithm>
#include <functional>
#include <format>
#include <iostream>
#include <string>
#include <codecvt>
#include <chrono>
#include <filesystem>

#include "vec.h"
#include "ui.h"
#include "raylib.h"

using namespace std::chrono_literals;

const int window_width = 1280;
const int window_height = 960;

const std::chrono::duration<double> hit_range = 100ms;
const std::chrono::duration<double> perfecct_range = 30ms;

const float circle_radius = 0.1f;
const float circle_outer_radius = 0.11f;

Texture2D inner_drum;
Texture2D outer_drum;

enum class NoteType {
    kat,
    don,
};

struct Note {
    std::chrono::duration<double> time;
    NoteType type;
};


class Cam {
public:
    Vec2 position;
    Vec2 bounds;
    
    Vec2 world_to_screen(Vec2 pos) const;
    float world_to_screen_scale(float length) const;
    Vec2 screen_to_world(Vec2 pos) const;
};

Vec2 Cam::world_to_screen(Vec2 pos) const {
    Vec2 temp = {pos.x - position.x, position.y - pos.y};
    Vec2 normalized = temp / bounds + 0.5;
        
    return Vec2 {
        normalized.x * (float)GetScreenWidth(),
        normalized.y * (float)GetScreenHeight()
    };
}

float Cam::world_to_screen_scale(float length) const {
    return length / bounds.y * GetScreenHeight();

}

Vec2 Cam::screen_to_world(Vec2 pos) const {
    Vec2 normalized = {pos.x / GetScreenWidth(), pos.y / GetScreenHeight()};
    Vec2 offset = (normalized - Vec2{0.5, 0.5}) * bounds;
    return {offset.x + position.x, position.y - offset.y};
}

void draw_map(const std::vector<Note>& map, const Cam& cam, int current_note) {
    if (map.size() == 0) {
        return;
    }

    int right = map.size() - 1;
    constexpr float circle_padding = 0.2f;
    float right_bound = cam.position.x + cam.bounds.x / 2 + circle_padding;

    for (int i = current_note; i < map.size(); i++) {
        if (map[i].time.count() >= right_bound) {
            right = i - 1;
            break;
        }
    }

    for(int i = right; i >= current_note; i--) {
        const Note& note = map[i];
        Vec2 circle_pos = cam.world_to_screen({(float)note.time.count(), 0});
        Color color = (note.type == NoteType::don) ? RED : BLUE;

        DrawCircle(circle_pos.x, circle_pos.y, cam.world_to_screen_scale(circle_outer_radius), WHITE);
        DrawCircle(circle_pos.x, circle_pos.y, cam.world_to_screen_scale(circle_radius), color);
    }
}

const float particle_duration = 1.0f;

struct Particle {
    Vec2 position;
    Vec2 velocity;
    float scale;
    NoteType type;
    std::chrono::duration<double> start;
};

void draw_particles(const Cam& cam, const std::vector<Particle>& particles, std::chrono::duration<double> now) {
    float outer_radius = cam.world_to_screen_scale(circle_outer_radius);
    float inner_radius = cam.world_to_screen_scale(circle_radius);
    for (auto& p : particles) {
        Vec2 pos = cam.world_to_screen(p.position);

        Color color;

        if (p.type == NoteType::don) {
            color = RED;
        } else {
            color = BLUE;
        }

        uint8_t alpha = (p.start - now).count() / particle_duration * 255;
        color.a = alpha;

        //DrawCircle(pos.x, pos.y, outer_radius, Color{255,255,255,alpha});
        DrawCircle(pos.x, pos.y, inner_radius, color);
    }
}

enum class Input {
    don_left,
    don_right,
    kat_left,
    kat_right,
};

struct InputRecord {
    Input type;
    float time;
};


class Game {
public:
    Game();
    void update(std::chrono::duration<double> delta_time);
private:
    Sound don_sound = LoadSound("don.wav");
    Sound kat_sound = LoadSound("kat.wav");
    
    Cam cam = {{0,0}, {4,3}};

    std::vector<Note> map{};
    int current_note = 0;

    std::vector<Particle> particles;

    int score = 0;

    std::chrono::time_point<std::chrono::steady_clock> start = std::chrono::high_resolution_clock::now();

    std::vector<InputRecord> inputs;
};

Game::Game() {
    for (int i = 0; i < 100; i++) {
        map.push_back({
            (0.2371s / 2) * i + 0.4743s,
            NoteType::don
        });
    }
}

const float input_indicator_duration = 0.1f;

void Game::update(std::chrono::duration<double> delta_time) {
    auto now = std::chrono::high_resolution_clock::now();
    std::chrono::duration<double> elapsed = now - start;

    //bool inputs[4] = { false, false, false, false };
    //std::array<bool, 4> inputs = { false, false, false, false };
    std::vector<NoteType> pressed;


    if (IsKeyPressed(KEY_X)) {
        PlaySound(don_sound);
        inputs.push_back(InputRecord{ Input::don_left, (float) elapsed.count() });

        pressed.push_back(NoteType::don);
    }

    if (IsKeyPressed(KEY_PERIOD)) {
        PlaySound(don_sound);
        inputs.push_back(InputRecord{ Input::don_right, (float)elapsed.count() });

        pressed.push_back(NoteType::don);
    }

    if (IsKeyPressed(KEY_Z)) {
        inputs.push_back(InputRecord{ Input::kat_left, (float)elapsed.count() });
        PlaySound(kat_sound);

        pressed.push_back(NoteType::kat);
    }

    if (IsKeyPressed(KEY_SLASH)) {
        inputs.push_back(InputRecord{ Input::kat_right, (float)elapsed.count() });
        PlaySound(kat_sound);

        pressed.push_back(NoteType::kat);
    }

    if (current_note < map.size()) {
        for (auto& note : pressed) {
            double hit_normalized = (elapsed - map[current_note].time) / hit_range + 0.5;
            if (hit_normalized >= 0 && hit_normalized < 1) {
                if (map[current_note].type == note) {
                    score += 300;
                    //particles.push_back(Particle{ Vec2{(float)elapsed.count(), 0}, {0,1},1, note.type, elapsed });
                    current_note++;
                }
            }

        }

        if (elapsed > map[current_note].time - (hit_range / 2)) {
            //PlaySound(don_sound);
            particles.push_back(Particle{ Vec2{(float)elapsed.count(), 0}, {0,1}, 1, map[current_note].type, elapsed });
            current_note++;
        }
    }

    cam.position.x = elapsed.count();

    for (int i = particles.size() - 1; i >= 0; i--) {
        Particle& p = particles[i];
        p.position += p.velocity * delta_time.count();

        if (elapsed - p.start > 1s) {
            particles.erase(particles.begin() + i);
        }
    }

    UI ui;

    Style style{};
    style.anchor = { 1,0 };
    ui.begin_group(style);
    std::string score_text = std::to_string(score);
    ui.rect(score_text.data());
    ui.end_group();



    BeginDrawing();

    ClearBackground(BLANK);
    DrawRectangle(0, 0, 1280, 960, BLACK);

    float x = elapsed.count();
    Vec2 p1 = cam.world_to_screen({ x, 0.5f });
    Vec2 p2 = cam.world_to_screen({ x, -0.5f });
    DrawLine(p1.x, p1.y, p2.x, p2.y, YELLOW);

    ui.draw();

    Vector2 drum_pos = { 0, (window_height - inner_drum.height) / 2};
    Vector2 right_pos = drum_pos;
    right_pos.x += inner_drum.width;
	Rectangle rect{ 0, 0, inner_drum.width, inner_drum.height };
    Rectangle flipped_rect = rect;
    flipped_rect.width *= -1;

    for (int i = inputs.size() - 1; i >= 0; i--) {
        const InputRecord& input = inputs[i];
        if (elapsed.count() - input.time > input_indicator_duration) {
            break;
        }

        switch (input.type) {
        case Input::don_left:
            DrawTextureRec(inner_drum, rect, drum_pos, WHITE);
            break;
        case Input::don_right:
            DrawTextureRec(inner_drum, flipped_rect, right_pos, WHITE);
            break;
        case Input::kat_left:
			DrawTextureRec(outer_drum, flipped_rect, drum_pos, WHITE);
            break;
        case Input::kat_right:
			DrawTextureRec(outer_drum, rect, right_pos, WHITE);
            break;
        }
    }


    Vec2 target = cam.world_to_screen(cam.position);
    DrawCircle(target.x, target.y, 50, WHITE);
    DrawText((std::to_string(delta_time.count() * 1000) + " ms").data(), 100, 100, 24, WHITE);
    draw_map(map, cam, current_note);

    draw_particles(cam, particles, elapsed);

    EndDrawing();

}

void draw_map_editor(const std::vector<Note>& map, const Cam& cam, int current_note) {
    if (map.size() == 0) {
        return;
    }

    int right = map.size() - 1;
    int left = 0;
    constexpr float circle_padding = 0.2f;
    float right_bound = cam.position.x + cam.bounds.x / 2 + circle_padding;
    float left_bound = cam.position.x - (cam.bounds.x / 2 + circle_padding);

    for (int i = current_note; i < map.size(); i++) {
        if (map[i].time.count() >= right_bound) {
            right = i - 1;
            break;
        }
    }
    for (int i = current_note; i >= 0; i--) {
        if (i >= map.size()) {
            continue;
        }
        if (map[i].time.count() <= left_bound) {
            left = i + 1;
            break;
        }
    }

    for(int i = right; i >= left; i--) {
        const Note& note = map[i];
        Vec2 circle_pos = cam.world_to_screen({(float)note.time.count(), 0});
        Color color = (note.type == NoteType::don) ? RED : BLUE;

        DrawCircle(circle_pos.x, circle_pos.y, cam.world_to_screen_scale(circle_outer_radius), WHITE);
        DrawCircle(circle_pos.x, circle_pos.y, cam.world_to_screen_scale(circle_radius), color);
    }
}

enum class EditorMode {
    select,
    insert,
};

class Editor {
public:
    void update_editor(std::chrono::duration<double> delta_time);
    void init();
private:
    Cam cam = {{0,0}, {2,3}};

    EditorMode mode;
    std::vector<int> selected;
    NoteType note_type;

    std::vector<Note> map;
    std::chrono::duration<double> offset;
    float bpm = 253;

    double quarter_interval = 60 / bpm / 4;
    double collision_range = quarter_interval / 2;

    bool paused = true;

    int current_note = -1;

    UI ui;

    Music music;

    Sound don_sound = LoadSound("don.wav");
    Sound kat_sound = LoadSound("kat.wav");
};

void Editor::init() {
    music = LoadMusicStream("kinoko.mp3");
    SetMusicVolume(music, 0.2f);

    PlayMusicStream(music);
    offset = std::chrono::duration<double>(0.994);

    PauseMusicStream(music);
}

void add_note(std::vector<Note>& map, Note note) {
    auto cmp = [](Note l, Note r) { return l.time < r.time; };
    map.insert(std::upper_bound(map.begin(), map.end(), note, cmp), note);
}

void Editor::update_editor(std::chrono::duration<double> delta_time) {
    UpdateMusicStream(music);

    float elapsed = GetMusicTimePlayed(music);

    if (IsMouseButtonPressed(0)) {
        Vec2 cursor_pos = cam.screen_to_world({(float)GetMouseX(), (float)GetMouseY()});

        int i = std::round((cursor_pos.x - offset.count()) / quarter_interval);
        auto time = std::chrono::duration<double>(offset.count() + i * quarter_interval);

        bool collision = false;
        double diff;
        for (int i = 0; i < map.size(); i++) {
            diff = std::abs((map[i].time - time).count());
            if (diff < collision_range) {
                collision = true;
                break;
            }
        }

        if (!collision) {
            add_note(map, Note{time, note_type});

            if (time.count() < elapsed) {
                current_note++;
            }
        }
    }

    if (IsKeyPressed(KEY_R)) {
        if (note_type == NoteType::don) {
            note_type = NoteType::kat;
        } else {
            note_type = NoteType::don;
        }
    }

    if (IsKeyPressed(KEY_SPACE)) {
        if (paused) {
            ResumeMusicStream(music);
            auto cmp = [](float current_time, Note& note) { return current_time < note.time.count(); };
            current_note = std::upper_bound(map.begin(), map.end(), elapsed, cmp) - map.begin();
            std::cout << current_note << '\n';
        } else {
            PauseMusicStream(music);
        }

        paused = !paused;
    }

    if (!paused && current_note < map.size() && elapsed >= map[current_note].time.count()) {
        if (map[current_note].type == NoteType::don) {
            PlaySound(don_sound);
        } else {
            PlaySound(kat_sound);
        }

        current_note++;
    }

    if (IsKeyPressed(KEY_A) && GetMusicTimePlayed(music) != 0.0f) {
        SeekMusicStream(music, 1.0f);
        std::cout << GetMusicTimePlayed(music) << '\n';
    }

    cam.position.x = elapsed;

    float wheel = GetMouseWheelMove();
    if (wheel != 0) {
        float seek = std::clamp(elapsed - wheel * 0.4f, 0.0f, GetMusicTimeLength(music));
        SeekMusicStream(music, seek);
        elapsed = GetMusicTimePlayed(music);

        // if (map.size() > 0 && elapsed < map[current_note].time.count()) {
        //     for (int i = current_note - 1; i >= 0; i--) {
        //         if (elapsed > map[i].time.count()) {
        //             current_note = i + 1;
        //             break;
        //         }
        //     }
        // } else {

        // }
        // auto cmp = [](float current_time, Note& note) { return current_time < note.time.count(); };
        // current_note = std::upper_bound(map.begin(), map.end(), elapsed, cmp) - map.begin();
        std::cout << current_note << '\n';
    }

    //ui.input();

    //ui.begin_group(Style{ {0,1} });
    //std::string time = std::to_string(cam.position.x) + " s";
    //ui.rect(time.data());
    //ui.slider(elapsed / GetMusicTimeLength(music), [&](float fraction) {
    //    SeekMusicStream(music, fraction * GetMusicTimeLength(music));
    //});

    //ui.end_group();

    //ui.begin_group(Style{ {1,1} });
    //auto frame_time = std::to_string(((float)std::chrono::duration_cast<std::chrono::microseconds>(delta_time).count()) / 1000) + " ms";
    //ui.rect(frame_time.data());
    //ui.end_group();

    BeginDrawing();
    ClearBackground(BLACK);

	float right_bound = cam.position.x + cam.bounds.x / 2;
	float left_bound = cam.position.x - cam.bounds.x / 2;

    int start = (left_bound - offset.count()) / quarter_interval + 2;
    int end = (right_bound - offset.count()) / quarter_interval - 1;

    for (int i = start; i < end; i++) {
        float x = offset.count() + i * quarter_interval;

        if (x > right_bound) {
            break;
        }

        float height;
        Color color;
        if (i % 4 == 0) {
            height = 0.2;
            color = WHITE;
        } else {
            height = 0.1;
            color = RED;
        }
        
        Vec2 p1 = cam.world_to_screen({x, 0});
        Vec2 p2 = cam.world_to_screen({x, height});

        DrawLine(p1.x, p1.y, p2.x, p2.y, color);
    }

    Vec2 p1 = cam.world_to_screen(cam.position);
    Vec2 p2 = cam.world_to_screen(cam.position + Vec2{0,0.6});

    DrawLine(p1.x, p1.y, p2.x, p2.y, YELLOW);

    draw_map_editor(map, cam, current_note);


    //ui.draw();
    EndDrawing();
}


enum class View {
    main,
    map_select,
};

class MainMenu {
public:
    void update(std::function<void()> callback);

private:
    UI ui;
    View current_view = View::main;
};

void MainMenu::update(std::function<void()> callback) {
    if (IsKeyPressed(KEY_ONE)) {
        callback();
        std::cout << "fskadfh\n";
        return;
    }

    ui.input();


    switch (current_view) {
        case View::main:
            ui.begin_group({});
            ui.button("Play", [&]() {
                current_view = View::map_select;
                std::cout << "kfsahdfhj\n";
            });
            ui.rect("Settings");
            ui.rect("Exit");

            ui.end_group();
            break;
        case View::map_select:
            ui.rect("askjldhf");
            if (IsKeyPressed(KEY_ESCAPE)) {
                current_view = View::main;
            }
            break;
    }

    BeginDrawing();
    ClearBackground(BLACK);
    DrawText("editor", 400, 300, 24, WHITE);

    ui.draw();
    EndDrawing();
}


enum class Context {
    Menu,
    Editor,
    Game,
};

void run() {
    InitWindow(window_width, window_height, "taiko");
    
    auto last_frame = std::chrono::high_resolution_clock::now();
    
    InitAudioDevice();
    SetExitKey(KEY_NULL);

    SetMasterVolume(0.5f);

    inner_drum = LoadTexture("drum-inner.png");
    outer_drum = LoadTexture("drum-outer.png");
    
    float pos = 400;
    auto start = std::chrono::high_resolution_clock::now();

    MainMenu menu;

    Editor app;
    
    Game game;

    app.init();

    Context context = Context::Game;

    auto to_editor = [&]() {
        context = Context::Editor;
    };

    using namespace std::chrono;
    
    while (!WindowShouldClose()) {

        //BeginDrawing();
        //ClearBackground(BLACK);

        //DrawCircle(600, 500, 100, Color{ 0, 0, 255, 128 });
        //DrawCircle(500, 500, 100, Color{ 255, 0, 0, 128 });

        //EndDrawing();

        auto now = high_resolution_clock::now();
        duration<double> delta_time = now - last_frame;

        switch (context) {
            case Context::Menu:
                menu.update(to_editor);
                break;
            case Context::Editor:
                app.update_editor(delta_time);
                break;
            case Context::Game:
                game.update(delta_time);
                break;
        }

        last_frame = now;
    }
    
    CloseWindow();
}

int main() {
    run();
}