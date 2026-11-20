#define DOCTEST_CONFIG_IMPLEMENT_WITH_MAIN
#include <doctest.h>

#include "app/widgets_app.hpp"

#include <prism/app/widget_tree.hpp>
#include <prism/raster/fonts.hpp>

namespace {
void press_increment(app::WidgetsModel &model)
{
    auto button = model.increment.get();
    ++button.click_count;
    model.increment.set(button);
}
} // namespace

TEST_CASE("widgets app: +1 counts the clicks")
{
    app::WidgetsModel model;
    press_increment(model);
    press_increment(model);
    CHECK(model.counter.get() == 2);
}

TEST_CASE("widgets app: the view builds a snapshot")
{
    app::WidgetsModel model;
    prism::app::WidgetTree tree{model};
    prism::raster::Fonts fonts;
    tree.set_text_measurer(&fonts);
    auto snap = tree.build_snapshot(480, 272, 1);
    REQUIRE(snap != nullptr);
    CHECK_FALSE(snap->draw_lists.empty());
}
