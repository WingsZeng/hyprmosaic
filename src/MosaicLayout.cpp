#include "MosaicLayout.hpp"
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
#include <iterator>
#include <optional>
#include <ranges>
#include <utility>
#include <vector>

namespace {
using namespace Layout;
using namespace Hyprdrive::MosaicPack;
using Hyprmosaic::Config::FocusMode;

class CMosaicLayout final : public ITiledAlgorithm {
public:
  CMosaicLayout() : m_focusMode(Hyprmosaic::Config::focusMode()) {
    m_focusListener = Event::bus()->m_events.window.active.listen(
        [this](PHLWINDOW window, Desktop::eFocusReason) {
          if (!window || !m_parent->space()->workspace()->visible())
            return;

          const auto target = window->layoutTarget();
          if (findTarget(target) == m_targets.end())
            return;

          switch (m_focusMode) {
          case FocusMode::Smart: {
            const auto from = m_lastFocus.lock();
            if (from == target || findTarget(from) == m_targets.end()) {
              if (!fullyVisible(target))
                revealTarget(target, {});
            } else if (!fullyVisible(target) || !fullyVisible(from))
              revealTarget(target, from);
            break;
          }
          case FocusMode::Center:
            centerTarget(target);
            break;
          case FocusMode::Count:
            std::unreachable();
          }

          m_lastFocus = target;
        });
  }

  void newTarget(SP<ITarget> target) override {
    m_targets.push_back({target, defaultSize()});
    recalculate();
  }

  void movedTarget(SP<ITarget> target, std::optional<Vector2D>) override {
    auto size = target->position().size();
    if (size.x <= 0 || size.y <= 0) {
      const auto desired = target->desiredGeometry();
      size = desired ? desired->size : defaultSize();
    }
    m_targets.push_back({target, size});
    recalculate();
  }

  void removeTarget(SP<ITarget> target) override {
    if (const auto it = findTarget(target); it != m_targets.end())
      m_targets.erase(it);
    recalculate();
  }

  void resizeTarget(const Vector2D &delta, SP<ITarget> target,
                    eRectCorner corner) override {
    const auto it = findTarget(target);
    if (it == m_targets.end())
      return;

    auto growth = delta;
    if (corner != CORNER_NONE) {
      growth.x = edgeLeft(corner) ? -delta.x : edgeRight(corner) ? delta.x : 0;
      growth.y = edgeTop(corner) ? -delta.y : edgeBottom(corner) ? delta.y : 0;
    }

    it->size.x = std::max(1.0, it->size.x + growth.x);
    it->size.y = std::max(1.0, it->size.y + growth.y);
    recalculate();
  }

  void recalculate(eRecalculateReason = RECALCULATE_REASON_UNKNOWN) override {
    const auto sizes =
        m_targets | std::views::transform([](const auto &sizedTarget) {
          return SRect{0, 0, sizedTarget.size.x, sizedTarget.size.y};
        }) |
        std::ranges::to<std::vector<SRect>>();

    const auto workArea = m_parent->space()->workArea();
    packCanvasMemo(m_memo, sizes,
                   SRect{workArea.x, workArea.y, workArea.w, workArea.h},
                   std::nullopt, 0, true);

    updateCanvasTranslation();
  }

  SP<ITarget> getNextCandidate(SP<ITarget> old) override {
    const auto it = findTarget(old);
    if (it == m_targets.end())
      return nullptr;

    const auto previous =
        it == m_targets.begin() ? std::prev(m_targets.end()) : std::prev(it);
    return previous == it ? nullptr : previous->target.lock();
  }

  Config::ErrorResult layoutMsg(const std::string_view &message) override {
    if (message == "togglefocus") {
      switch (m_focusMode) {
      case FocusMode::Smart:
        m_focusMode = FocusMode::Center;
        break;
      case FocusMode::Center:
        m_focusMode = FocusMode::Smart;
        break;
      case FocusMode::Count:
        std::unreachable();
      }
      updateCanvasTranslation();
    }
    return {};
  }

  std::optional<Vector2D> predictSizeForNewTarget() override {
    return std::nullopt;
  }

  void swapTargets(SP<ITarget> a, SP<ITarget> b) override {
    const auto [first, second] = std::pair{findTarget(a), findTarget(b)};
    if (first == m_targets.end() || second == m_targets.end())
      return;

    std::iter_swap(first, second);
    recalculate();
  }

  void moveTargetInDirection(SP<ITarget>, Math::eDirection, bool) override {}

private:
  struct SSizedTarget {
    WP<ITarget> target;
    Vector2D size;
  };

  auto findTarget(this auto &self, const SP<ITarget> &target)
      -> decltype(self.m_targets.begin()) {
    if (!target)
      return self.m_targets.end();

    return std::ranges::find(
        self.m_targets, target,
        [](const auto &sizedTarget) { return sizedTarget.target.lock(); });
  }

  const SRect *getTargetRect(const SP<ITarget> &target) const {
    const auto it = findTarget(target);
    if (it == m_targets.end())
      return nullptr;

    return &m_memo.out.rects[std::distance(m_targets.begin(), it)];
  }

  bool fullyVisible(const SP<ITarget> &target) const {
    const auto rect = getTargetRect(target);
    if (!rect)
      return false;

    const auto position = target->position().pos();
    const auto workArea = m_parent->space()->workArea();
    return position.x >= workArea.x - 0.5 && position.y >= workArea.y - 0.5 &&
           position.x + rect->w <= workArea.x + workArea.w + 0.5 &&
           position.y + rect->h <= workArea.y + workArea.h + 0.5;
  }

  void centerTarget(const SP<ITarget> &target) {
    const auto rect = getTargetRect(target);
    if (!rect)
      return;

    translateCanvas(Vector2D{rect->x + rect->w / 2.0, rect->y + rect->h / 2.0},
                    m_parent->space()->workArea().middle());
  }

  void revealTarget(const SP<ITarget> &target, const SP<ITarget> &from) {
    const auto rect = getTargetRect(target);
    if (!rect)
      return;

    const auto workArea = m_parent->space()->workArea();
    const auto view =
        canvasView(m_memo.out.w, m_memo.out.h,
                   SRect{workArea.x, workArea.y, workArea.w, workArea.h});

    std::optional<SSpan> fromX, fromY;
    if (const auto fromRect = getTargetRect(from)) {
      fromX = SSpan{fromRect->x, fromRect->w};
      fromY = SSpan{fromRect->y, fromRect->h};
    }

    static const std::vector<SSpan> NO_NEIGHBOURS;
    const Vector2D offset{
        smartRevealAxis(workArea.w, view.maxX, {rect->x, rect->w}, fromX,
                        NO_NEIGHBOURS, 0, 0),
        smartRevealAxis(workArea.h, view.maxY, {rect->y, rect->h}, fromY,
                        NO_NEIGHBOURS, 0, 0)};
    translateCanvas(offset, {view.originX, view.originY});
  }

  void updateCanvasTranslation() {
    const auto window = Desktop::focusState()->window();
    const auto target = window ? window->layoutTarget() : SP<ITarget>{};
    if (findTarget(target) != m_targets.end()) {
      switch (m_focusMode) {
      case FocusMode::Smart:
        revealTarget(target, {});
        break;
      case FocusMode::Center:
        centerTarget(target);
        break;
      case FocusMode::Count:
        std::unreachable();
      }

      if (m_lastFocus.expired())
        m_lastFocus = target;
    } else {
      translateCanvas(Vector2D{m_memo.out.w / 2.0, m_memo.out.h / 2.0},
                      m_parent->space()->workArea().middle());
    }
  }

  Vector2D defaultSize() const {
    return Hyprmosaic::Config::defaultSize(
        m_parent->space()->workArea().size());
  }

  // Translate canvasPoint in packer-local coordinates onto globalPoint.
  void translateCanvas(const Vector2D &canvasPoint,
                       const Vector2D &globalPoint) const {
    assert(m_targets.size() == m_memo.out.rects.size());

    const auto origin = globalPoint - canvasPoint;
    for (const auto &[sizedTarget, rect] :
         std::views::zip(m_targets, m_memo.out.rects)) {
      const auto target = sizedTarget.target.lock();
      assert(target);
      target->setPositionGlobal(
          CBox{origin.x + rect.x, origin.y + rect.y, rect.w, rect.h});
    }
  }

  std::vector<SSizedTarget> m_targets;
  SCanvasMemo m_memo;
  WP<ITarget> m_lastFocus;
  FocusMode m_focusMode;
  CHyprSignalListener m_focusListener;
};
} // namespace

UP<Layout::ITiledAlgorithm> Hyprmosaic::makeLayout() {
  return makeUnique<CMosaicLayout>();
}

const std::type_info *Hyprmosaic::layoutType() noexcept {
  return &typeid(CMosaicLayout);
}
