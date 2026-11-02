#include "esp_visuals.h"
#include "esp_elements.h"
#include "hero_catalog.h"
#include <cassert>
#include <cmath>
#include <cstdio>
#include <limits>
#include <vector>

namespace {
struct Geometry {
    std::vector<ImDrawVert> vertices;
    std::vector<unsigned> indices;
    bool empty() const { return vertices.empty() && indices.empty(); }
};

template<class Emit> Geometry capture(ImDrawList* draw, Emit emit) {
    const int first_vertex = draw->VtxBuffer.Size;
    const int first_index = draw->IdxBuffer.Size;
    emit();
    Geometry result;
    for (int i = first_vertex; i < draw->VtxBuffer.Size; ++i)
        result.vertices.push_back(draw->VtxBuffer[i]);
    for (int i = first_index; i < draw->IdxBuffer.Size; ++i) {
        const auto index = static_cast<unsigned>(draw->IdxBuffer[i]);
        // These small fixtures never enable or reach ImGui's 64K vertex offset.
        assert(index >= static_cast<unsigned>(first_vertex));
        assert(index < static_cast<unsigned>(draw->VtxBuffer.Size));
        result.indices.push_back(index - static_cast<unsigned>(first_vertex));
    }
    assert(result.indices.size() % 3 == 0);
    return result;
}

bool approximately_equal(float a, float b) { return std::fabs(a - b) < .001f; }

Geometry joined(Geometry first, const Geometry& second) {
    const auto offset = static_cast<unsigned>(first.vertices.size());
    first.vertices.insert(first.vertices.end(), second.vertices.begin(), second.vertices.end());
    for (auto index : second.indices) first.indices.push_back(offset + index);
    return first;
}

void same_geometry(const Geometry& actual, const Geometry& expected) {
    assert(actual.vertices.size() == expected.vertices.size());
    assert(actual.indices == expected.indices);
    for (std::size_t i = 0; i < actual.vertices.size(); ++i) {
        const auto& a = actual.vertices[i];
        const auto& e = expected.vertices[i];
        assert(approximately_equal(a.pos.x, e.pos.x) && approximately_equal(a.pos.y, e.pos.y));
        assert(approximately_equal(a.uv.x, e.uv.x) && approximately_equal(a.uv.y, e.uv.y) && a.col == e.col);
    }
}

bool has_color(const Geometry& geometry, ImU32 color) {
    for (const auto& vertex : geometry.vertices) if (vertex.col == color) return true;
    return false;
}

void color_bounds(const Geometry& geometry, ImU32 color, float x1, float y1, float x2, float y2) {
    bool found = false;
    ImVec2 lower{}, upper{};
    for (const auto& vertex : geometry.vertices) if (vertex.col == color) {
        if (!found) lower = upper = vertex.pos;
        else {
            lower.x = std::fmin(lower.x, vertex.pos.x);
            lower.y = std::fmin(lower.y, vertex.pos.y);
            upper.x = std::fmax(upper.x, vertex.pos.x);
            upper.y = std::fmax(upper.y, vertex.pos.y);
        }
        found = true;
    }
    assert(found && approximately_equal(lower.x, x1) && approximately_equal(lower.y, y1));
    assert(approximately_equal(upper.x, x2) && approximately_equal(upper.y, y2));
}

float edge(ImVec2 a, ImVec2 b, ImVec2 p) {
    return (b.x - a.x) * (p.y - a.y) - (b.y - a.y) * (p.x - a.x);
}

bool covers(const Geometry& geometry, ImVec2 point, ImU32 color) {
    for (std::size_t i = 0; i < geometry.indices.size(); i += 3) {
        const auto& a = geometry.vertices[geometry.indices[i]];
        const auto& b = geometry.vertices[geometry.indices[i + 1]];
        const auto& c = geometry.vertices[geometry.indices[i + 2]];
        if (a.col != color || b.col != color || c.col != color) continue;
        if (std::fabs(edge(a.pos, b.pos, c.pos)) < .0001f) continue;
        const float ab = edge(a.pos, b.pos, point);
        const float bc = edge(b.pos, c.pos, point);
        const float ca = edge(c.pos, a.pos, point);
        const bool negative = ab < -.001f || bc < -.001f || ca < -.001f;
        const bool positive = ab > .001f || bc > .001f || ca > .001f;
        if (!(negative && positive)) return true;
    }
    return false;
}

void health_bar(const Geometry& geometry, float x, float fill_top, ImU32 fill, ImU32 background) {
    assert(!geometry.empty());
    for (const auto& vertex : geometry.vertices)
        assert(vertex.col == fill || vertex.col == background);
    color_bounds(geometry, background, x, 100, x + 4, 400);
    color_bounds(geometry, fill, x, fill_top, x + 4, 400);
    assert(covers(geometry, {x + 2, 110}, background));
    assert(covers(geometry, {x + 2, (fill_top + 400) * .5f}, fill));
    if (fill_top > 100) assert(!covers(geometry, {x + 2, fill_top - 1}, fill));
}
}

int main() {
    cfg.register_all();
    auto low = esp_health_color(0, true), mid = esp_health_color(.5f, true), high = esp_health_color(1, true);
    assert(low.r > low.g && mid.r > .9f && mid.g > .9f && high.g > high.r);
    auto left = esp_health_color(.49999f, true), right = esp_health_color(.50001f, true);
    assert(std::fabs(left.r - right.r) < .001f && std::fabs(left.g - right.g) < .001f);
    cfg.find("esp.fade_distance")->b = true;
    cfg.find("esp.fade_start")->f = 20;
    cfg.find("esp.fade_end")->f = 100;
    assert(esp_distance_alpha(10) == 1 && esp_distance_alpha(60) == 1 && esp_distance_alpha(1000) == 1);
    cfg.find("esp.ultimate_position")->i = 2;
    cfg.find("esp.abilities_position")->i = 3;
    {
        Settings::VisualScope team(true);
        assert(!cfg.B("esp.enabled"));
        cfg.find("esp.health_style")->i = 2;
        cfg.find("esp.box_visible")->c = {.1f, .2f, .3f, 1};
    }
    assert(cfg.I("esp.health_style") == 0);
    cfg.find("esp.box_visible")->c = {.7f, .8f, .9f, 1};
    { Settings::VisualScope team(true); assert(cfg.I("esp.health_style") == 2 && cfg.C("esp.box_visible").r == .1f); }
    assert(cfg.save("visual_test.json"));
    cfg = Settings{};
    cfg.register_all();
    assert(cfg.load("visual_test.json"));
    { Settings::VisualScope team(true); assert(cfg.I("esp.health_style") == 2 && cfg.C("esp.box_visible").r == .1f); }
    assert(cfg.I("esp.ultimate_position") == 2 && cfg.I("esp.abilities_position") == 3);

    auto* context = ImGui::CreateContext();
    auto& io = ImGui::GetIO();
    io.DisplaySize = {800, 600};
    io.DeltaTime = .016f;
    io.IniFilename = nullptr;
    unsigned char* pixels;
    int width, height;
    io.Fonts->GetTexDataAsRGBA32(&pixels, &width, &height);
    ImGui::NewFrame();
    using namespace esp_elements;
    std::array<bool, Count> present{};
    present.fill(true);
    std::array<ImVec2, Count> sizes = {ImVec2{20, 12}, ImVec2{30, 13}, ImVec2{28, 12}, ImVec2{70, 15}, ImVec2{140, 42}};
    auto disjoint = [](const Blocks& blocks) {
        for (int i = 0; i < Count; ++i) for (int j = i + 1; j < Count; ++j) if (blocks[i].enabled && blocks[j].enabled) {
            auto a = blocks[i], b = blocks[j];
            bool hit = a.position.x < b.position.x + b.size.x && b.position.x < a.position.x + a.size.x && a.position.y < b.position.y + b.size.y && b.position.y < a.position.y + a.size.y;
            assert(!hit);
        }
    };
    for (auto key : position_keys) cfg.find(key)->i = 0;
    auto stacked = place(present, sizes, 100, 100, 160, 200);
    disjoint(stacked);
    cfg.find(position_keys[Souls])->i = 2;
    cfg.find(position_keys[Distance])->i = 3;
    cfg.find(position_keys[Ultimate])->i = 2;
    cfg.find(position_keys[Abilities])->i = 3;
    auto fitted = place(present, sizes, -60, -100, 250, 480);
    fit_preview(fitted, {0, 0}, {190, 400});
    disjoint(fitted);
    for (auto& b : fitted) assert(b.position.x >= -.01f && b.position.y >= -.01f && b.position.x + b.size.x <= 190.01f && b.position.y + b.size.y <= 400.01f);
    std::array<ability_tracking::View, 4> states{};
    for (auto& state : states) state.state = ability_tracking::State::Ready;
    states[1].state = ability_tracking::State::Cooldown;
    states[1].seconds = 123;
    auto with_timer = ability_row(states, 22, true), without_timer = ability_row(states, 22, false);
    assert(with_timer.captions[1] == "123s" && without_timer.captions[1].empty());
    assert(with_timer.size.y > without_timer.size.y && with_timer.slot_width >= ImGui::GetFont()->CalcTextSizeA(with_timer.caption_font, 1e10f, 0, "123s").x + 4);
    auto* draw = ImGui::GetBackgroundDrawList();
    draw_abilities(draw, {20, 20}, with_timer);

    cfg.find("esp.health")->b = true;
    cfg.find("esp.health_position")->i = 0;
    cfg.find("esp.health_gradient")->b = true;
    cfg.find("esp.health_low")->c = {1, 0, 0, 1};
    cfg.find("esp.health_mid")->c = {1, 1, 0, 1};
    cfg.find("esp.health_high")->c = {0, 1, 0, 1};
    auto health = [&](int style, float hp = 75, float max_hp = 100, bool visible = true, float alpha = 1) {
        cfg.find("esp.health_style")->i = style;
        return capture(draw, [&] { esp_health(draw, 100, 100, 180, 400, hp, max_hp, visible, alpha); });
    };
    const auto fill75 = IM_COL32(127, 255, 0, 255);
    const auto background = IM_COL32(8, 8, 12, 220);
    const auto bar = health(0), number = health(1), both = health(2);
    health_bar(bar, 92, 175, fill75, background);
    const auto text_size = ImGui::CalcTextSize("75");
    const auto label = capture(draw, [&] { draw->AddText({92 - text_size.x - 3, 175 - text_size.y * .5f}, fill75, "75"); });
    assert(!number.empty());
    same_geometry(number, label); // The numeric value and its anchor are observable output.
    same_geometry(both, joined(bar, number));
    health_bar(health(0, 25), 92, 325, IM_COL32(255, 127, 0, 255), background);
    health_bar(health(0, 50), 92, 250, IM_COL32(255, 255, 0, 255), background);
    health_bar(health(0, 100), 92, 100, IM_COL32(0, 255, 0, 255), background);
    health_bar(health(0, 75, 100, true, .5f), 92, 175, IM_COL32(127, 255, 0, 127), IM_COL32(8, 8, 12, 110));
    cfg.find("esp.health_position")->i = 1;
    health_bar(health(0), 184, 175, fill75, background);
    const auto right_number = health(1);
    const auto right_label = capture(draw, [&] { draw->AddText({191, 175 - text_size.y * .5f}, fill75, "75"); });
    same_geometry(right_number, right_label);
    cfg.find("esp.health_position")->i = 0;
    cfg.find("esp.health_gradient")->b = false;
    cfg.find("esp.health_visible")->c = {.25f, .5f, .75f, 1};
    cfg.find("esp.health_hidden")->c = {.75f, .5f, .25f, .5f};
    health_bar(health(0), 92, 175, IM_COL32(63, 127, 191, 255), background);
    health_bar(health(0, 75, 100, false, .5f), 92, 175, IM_COL32(191, 127, 63, 63), IM_COL32(8, 8, 12, 110));
    const float nan = std::numeric_limits<float>::quiet_NaN();
    const float infinity = std::numeric_limits<float>::infinity();
    for (int style = 0; style < 3; ++style) {
        assert(health(style, -1).empty() && health(style, 101).empty());
        assert(health(style, nan).empty() && health(style, infinity).empty());
        assert(health(style, 75, 0).empty() && health(style, 75, -1).empty());
        assert(health(style, 75, nan).empty() && health(style, 75, infinity).empty());
    }
    cfg.find("esp.health")->b = false;
    assert(health(0).empty() && health(1).empty() && health(2).empty());

    cfg.find("esp.box")->b = true;
    cfg.find("esp.box_width")->f = 2;
    cfg.find("esp.box_visible")->c = {1, 1, 1, 1};
    cfg.find("esp.box_fill_visible")->c = {.25f, .5f, .75f, .5f};
    auto box = [&](int style) {
        cfg.find("esp.box_style")->i = style;
        return capture(draw, [&] { esp_box(draw, 100, 100, 180, 400, true, .5f); });
    };
    const auto outline = box(0), corner = box(1), filled = box(2);
    const auto stroke = IM_COL32(255, 255, 255, 127), fill = IM_COL32(63, 127, 191, 63);
    assert(has_color(outline, stroke) && has_color(corner, stroke) && has_color(filled, stroke));
    assert(!covers(outline, {140, 250}, fill) && !covers(corner, {140, 250}, fill));
    assert(covers(filled, {140, 250}, fill));
    color_bounds(filled, fill, 100, 100, 180, 400);
    assert(covers(outline, {140, 100.5f}, stroke));
    assert(covers(corner, {110, 100.5f}, stroke) && !covers(corner, {140, 100.5f}, stroke));
    cfg.find("esp.box")->b = false;
    assert(box(0).empty() && box(1).empty() && box(2).empty());

    ImGui::EndFrame();
    ImGui::DestroyContext(context);
    assert(std::string(current_hero_name(11)) == "Dynamo" && std::string(current_hero_name(13)) == "Haze" && std::string(current_hero_name(3)) == "Vindicta");
    std::remove("visual_test.json");
    std::puts("PASS: health bar bounds/color/coverage, numeric value/anchors, combined display, side/alpha/invalid-input behavior, box fill/corner/outline geometry, legacy fade ignored, element layout/cooldown spacing, team settings and JSON persistence, current hero IDs");
}
