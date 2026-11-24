#include "MosaicLayout.hpp"

#include "CanvasDragController.hpp"
#include "Config.hpp"

#include <MosaicCanvas.hpp>

#include <hyprland/src/desktop/state/FocusState.hpp>
#include <hyprland/src/desktop/view/window/Window.hpp>
#include <hyprland/src/event/EventBus.hpp>
#include <hyprland/src/layout/algorithm/Algorithm.hpp>
#include <hyprland/src/layout/space/Space.hpp>
#include <hyprland/src/layout/target/Target.hpp>

#include <algorithm>
#include <cassert>
#include <charconv>
#include <cmath>
#include <iterator>
#include <optional>
#include <ranges>
#include <string_view>
#include <vector>

namespace {
using namespace Layout;
using namespace Hyprdrive::MosaicPack;
using namespace std::string_view_literals;

class CMosaicLayout final : public ITiledAlgorithm,
                            private Hyprmosaic::ICanvasDragTarget {
public:
  CMosaicLayout()
      : m_focusMode(Hyprmosaic::Config::focusMode()), m_dragController(*this) {
    m_focusListener = Event::bus()->m_events.window.active.listen(
        [this](PHLWINDOW window, Desktop::eFocusReason) {
          if (!window || !m_parent->space()->workspace()->visible())
            return;

          const auto target = window->layoutTarget();
          if (findTarget(target) == m_targets.end())
            return;

          const bool targetVisible = isFullyVisible(target);
          if (!(m_manual && targetVisible)) {
            m_manual = false;
            if (m_focusMode)
              centerTarget(target);
            else {
              const auto previous = m_previousFocus.lock();
              const bool previousVisible =
                  !previous || previous == target ||
                  findTarget(previous) == m_targets.end() ||
                  isFullyVisible(previous);
              if (!targetVisible || !previousVisible)
                fitTarget(target, previous);
            }
          }
          m_previousFocus = target;
        });
  }

  void newTarget(SP<ITarget> target) override {
    if (addTarget(target, defaultTargetSize())) {
      m_manual = false;
      recalculate();
    }
  }

  void movedTarget(SP<ITarget> target, std::optional<Vector2D>) override {
    auto size = target->position().size();
    if (size.x <= 0 || size.y <= 0) {
      const auto desired = target->desiredGeometry();
      size = desired && desired->size.x > 0 && desired->size.y > 0
                 ? desired->size
                 : defaultTargetSize();
    }
    if (addTarget(target, size)) {
      m_manual = false;
      recalculate();
    }
  }

  void removeTarget(SP<ITarget> target) override {
    const auto targetIt = findTarget(target);
    if (targetIt == m_targets.end())
      return;

    m_targets.erase(targetIt);
    m_manual = false;
    recalculate();
  }

  void resizeTarget(const Vector2D &delta, SP<ITarget> target,
                    eRectCorner corner) override {
    const auto targetIt = findTarget(target);
    if (targetIt == m_targets.end())
      return;

    Vector2D growth;
    if (corner == CORNER_NONE)
      growth = delta;
    else {
      growth.x = edgeLeft(corner) ? -delta.x : edgeRight(corner) ? delta.x : 0;
      growth.y = edgeTop(corner) ? -delta.y : edgeBottom(corner) ? delta.y : 0;
    }

    targetIt->size.x = std::max(MIN_TARGET_SIZE, targetIt->size.x + growth.x);
    targetIt->size.y = std::max(MIN_TARGET_SIZE, targetIt->size.y + growth.y);
    m_manual = false;
    recalculate();
  }

  void recalculate(eRecalculateReason = RECALCULATE_REASON_UNKNOWN) override {
    m_workArea = m_parent->space()->workArea();
    const auto sizes = m_targets |
                       std::views::transform([](const auto &target) {
                         return SRect{0, 0, target.size.x, target.size.y};
                       }) |
                       std::ranges::to<std::vector<SRect>>();
    packCanvasMemo(m_packMemo, sizes, SRect{0, 0, m_workArea.w, m_workArea.h},
                   std::nullopt, PACK_GAP, PACK_IN_TWO_DIMENSIONS);
    m_geometryDirty = true;

    if (m_manual && m_canvasOffset)
      translateCanvas(*m_canvasOffset);
    else {
      const auto window = Desktop::focusState()->window();
      const auto target = window ? window->layoutTarget() : SP<ITarget>{};
      if (findTarget(target) == m_targets.end())
        centerCanvas();
      else if (m_focusMode)
        centerTarget(target);
      else
        fitTarget(target, {});
    }
  }

  SP<ITarget> getNextCandidate(SP<ITarget> old) override {
    const auto targetIt = findTarget(old);
    if (targetIt == m_targets.end())
      return nullptr;

    const auto previous = targetIt == m_targets.begin()
                              ? std::prev(m_targets.end())
                              : std::prev(targetIt);
    return previous == targetIt ? nullptr : previous->target.lock();
  }

  Config::ErrorResult layoutMsg(const std::string_view &message) override {
    if (message == TOGGLE_FOCUS_MESSAGE) {
      if (m_manual) {
        m_manual = false;
        const auto window = Desktop::focusState()->window();
        const auto target = window ? window->layoutTarget() : SP<ITarget>{};
        if (findTarget(target) == m_targets.end())
          centerCanvas();
        else if (m_focusMode)
          centerTarget(target);
        else {
          const Vector2D centeredOffset =
              Vector2D{m_workArea.w / 2.0, m_workArea.h / 2.0} -
              Vector2D{m_packMemo.out.w / 2.0, m_packMemo.out.h / 2.0};
          if (isFullyVisible(target, centeredOffset))
            translateCanvas(centeredOffset);
          else
            fitTarget(target, {});
        }
      } else {
        m_focusMode = !m_focusMode;
        recalculate();
      }
    } else if (message == MOUSE_PAN_MESSAGE)
      m_dragController.beginDrag();
    else if (const auto delta = parsePanDelta(message))
      panCanvas(*delta);
    return {};
  }

  std::optional<Vector2D> predictSizeForNewTarget() override {
    return defaultTargetSize();
  }

  void swapTargets(SP<ITarget> a, SP<ITarget> b) override {
    const auto first = findTarget(a);
    const auto second = findTarget(b);
    if (first == m_targets.end() || second == m_targets.end())
      return;

    std::iter_swap(first, second);
    m_manual = false;
    recalculate();
  }

  void moveTargetInDirection(SP<ITarget>, Math::eDirection, bool) override {}

private:
  static constexpr auto TOGGLE_FOCUS_MESSAGE = "togglefocus"sv;
  static constexpr auto MOUSE_PAN_MESSAGE = "panmouse"sv;
  static constexpr auto PAN_MESSAGE_PREFIX = "pan "sv;
  static constexpr auto MIN_TARGET_SIZE = 1.0;
  static constexpr auto PACK_GAP = 0.0;
  static constexpr auto PACK_IN_TWO_DIMENSIONS = true;
  static constexpr auto VISIBILITY_TOLERANCE = 0.5;
  static constexpr auto REVEAL_MARGIN = 0.0;
  static constexpr auto NEIGHBOUR_PEEK = 0.0;

  static std::optional<Vector2D> parsePanDelta(const std::string_view message) {
    if (!message.starts_with(PAN_MESSAGE_PREFIX))
      return std::nullopt;

    auto arguments = message.substr(PAN_MESSAGE_PREFIX.size());
    const auto trimSpaces = [](std::string_view value) {
      while (!value.empty() && value.front() == ' ')
        value.remove_prefix(1);
      return value;
    };
    const auto parseNumber = [&](double &value) {
      arguments = trimSpaces(arguments);
      const auto [end, error] = std::from_chars(
          arguments.data(), arguments.data() + arguments.size(), value);
      if (error != std::errc{} || end == arguments.data())
        return false;
      arguments.remove_prefix(end - arguments.data());
      return std::isfinite(value);
    };

    Vector2D delta;
    if (!parseNumber(delta.x) || !parseNumber(delta.y) ||
        !trimSpaces(arguments).empty())
      return std::nullopt;
    return delta;
  }

  struct STarget {
    WP<ITarget> target;
    Vector2D size;
  };

  bool addTarget(const SP<ITarget> &target, const Vector2D &size) {
    if (findTarget(target) != m_targets.end())
      return false;

    m_targets.push_back({target, size});
    return true;
  }

  auto findTarget(this auto &self, const SP<ITarget> &target)
      -> decltype(self.m_targets.begin()) {
    if (!target)
      return self.m_targets.end();

    return std::ranges::find(self.m_targets, target, [](const auto &entry) {
      return entry.target.lock();
    });
  }

  const SRect *packedRect(const SP<ITarget> &target) const {
    const auto targetIt = findTarget(target);
    if (targetIt == m_targets.end())
      return nullptr;

    const auto index = std::distance(m_targets.cbegin(), targetIt);
    return &m_packMemo.out.rects[index];
  }

  bool isFullyVisible(const SP<ITarget> &target, const Vector2D &offset) const {
    const auto rect = packedRect(target);
    if (!rect)
      return false;

    const Vector2D targetPosition{offset.x + rect->x, offset.y + rect->y};
    return targetPosition.x >= -VISIBILITY_TOLERANCE &&
           targetPosition.y >= -VISIBILITY_TOLERANCE &&
           targetPosition.x + rect->w <= m_workArea.w + VISIBILITY_TOLERANCE &&
           targetPosition.y + rect->h <= m_workArea.h + VISIBILITY_TOLERANCE;
  }

  bool isFullyVisible(const SP<ITarget> &target) const {
    return m_canvasOffset && isFullyVisible(target, *m_canvasOffset);
  }

  void centerCanvas() {
    const Vector2D offset =
        Vector2D{m_workArea.w / 2.0, m_workArea.h / 2.0} -
        Vector2D{m_packMemo.out.w / 2.0, m_packMemo.out.h / 2.0};
    translateCanvas(offset);
  }

  void centerTarget(const SP<ITarget> &target) {
    const auto rect = packedRect(target);
    if (!rect)
      return;

    const Vector2D offset =
        Vector2D{m_workArea.w / 2.0, m_workArea.h / 2.0} -
        Vector2D{rect->x + rect->w / 2.0, rect->y + rect->h / 2.0};
    translateCanvas(offset);
  }

  void fitTarget(const SP<ITarget> &target, const SP<ITarget> &previous) {
    const auto targetRect = packedRect(target);
    assert(targetRect);

    const auto previousRect =
        previous == target ? nullptr : packedRect(previous);
    const auto view = canvasView(m_packMemo.out.w, m_packMemo.out.h,
                                 SRect{0, 0, m_workArea.w, m_workArea.h});

    std::optional<SSpan> previousX, previousY;
    if (previousRect) {
      previousX = SSpan{previousRect->x, previousRect->w};
      previousY = SSpan{previousRect->y, previousRect->h};
    }

    static const std::vector<SSpan> NO_NEIGHBOURS;
    const Vector2D viewportOffset{
        smartRevealAxis(m_workArea.w, view.maxX, {targetRect->x, targetRect->w},
                        previousX, NO_NEIGHBOURS, REVEAL_MARGIN,
                        NEIGHBOUR_PEEK),
        smartRevealAxis(m_workArea.h, view.maxY, {targetRect->y, targetRect->h},
                        previousY, NO_NEIGHBOURS, REVEAL_MARGIN,
                        NEIGHBOUR_PEEK)};
    translateCanvas(
        {view.originX - viewportOffset.x, view.originY - viewportOffset.y});
  }

  bool acceptsCanvasDrag(const Vector2D &position) const override {
    if (!m_parent || !m_parent->space() ||
        !m_parent->space()->workspace()->visible())
      return false;

    return m_workArea.containsPoint(position);
  }

  void panCanvas(const Vector2D &delta) override {
    if (!m_canvasOffset)
      return;

    const auto center = Vector2D{m_workArea.w / 2.0, m_workArea.h / 2.0};
    const auto requestedOffset = *m_canvasOffset + delta;
    const Vector2D offset{
        std::clamp(requestedOffset.x, center.x - m_packMemo.out.w, center.x),
        std::clamp(requestedOffset.y, center.y - m_packMemo.out.h, center.y)};
    if (offset == *m_canvasOffset)
      return;

    m_manual = true;
    translateCanvas(offset);
  }

  Vector2D defaultTargetSize() const {
    return Hyprmosaic::Config::defaultSize(
        m_parent->space()->workArea().size());
  }

  void translateCanvas(const Vector2D &offset) {
    if (!m_geometryDirty && m_canvasOffset && *m_canvasOffset == offset)
      return;

    assert(m_targets.size() == m_packMemo.out.rects.size());
    m_canvasOffset = offset;
    m_geometryDirty = false;
    const auto globalCanvasOrigin = m_workArea.pos() + offset;
    for (const auto &[targetEntry, rect] :
         std::views::zip(m_targets, m_packMemo.out.rects)) {
      const auto target = targetEntry.target.lock();
      assert(target);
      target->setPositionGlobal(CBox{globalCanvasOrigin.x + rect.x,
                                     globalCanvasOrigin.y + rect.y, rect.w,
                                     rect.h});
    }
  }

  std::vector<STarget> m_targets;
  SCanvasMemo m_packMemo;
  CBox m_workArea;
  std::optional<Vector2D> m_canvasOffset;
  WP<ITarget> m_previousFocus;
  bool m_focusMode;
  bool m_manual = false;
  bool m_geometryDirty = false;
  Hyprmosaic::CCanvasDragController m_dragController;
  CHyprSignalListener m_focusListener;
};
} // namespace

UP<Layout::ITiledAlgorithm> Hyprmosaic::makeLayout() {
  return makeUnique<CMosaicLayout>();
}

const std::type_info *Hyprmosaic::layoutType() noexcept {
  return &typeid(CMosaicLayout);
}
