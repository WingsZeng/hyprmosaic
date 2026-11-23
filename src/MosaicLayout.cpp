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
#include <optional>
#include <vector>

namespace {
using namespace Layout;
using namespace Hyprdrive::MosaicPack;

class CMosaicLayout final : public ITiledAlgorithm {
public:
  CMosaicLayout() : m_focusMode(Hyprmosaic::Config::focusModeEnabled()) {
    m_focusListener = Event::bus()->m_events.window.active.listen(
        [this](PHLWINDOW window, Desktop::eFocusReason) {
          if (!m_focusMode || !window ||
              !m_parent->space()->workspace()->visible())
            return;

          centerTarget(m_memo.out, window->layoutTarget());
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
    std::erase_if(m_targets, [&](const auto &sizedTarget) {
      return sizedTarget.target.lock() == target;
    });
    recalculate();
  }

  void resizeTarget(const Vector2D &delta, SP<ITarget> target,
                    eRectCorner corner) override {
    const auto it =
        std::ranges::find_if(m_targets, [&](const auto &sizedTarget) {
          return sizedTarget.target.lock() == target;
        });
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
    if (!m_parent || !m_parent->space())
      return;

    std::vector<SRect> sizes;
    sizes.reserve(m_targets.size());
    for (const auto &sizedTarget : m_targets)
      sizes.emplace_back(SRect{0, 0, sizedTarget.size.x, sizedTarget.size.y});

    const auto workArea = m_parent->space()->workArea();
    const auto packed = packCanvasMemo(
        m_memo, sizes, SRect{workArea.x, workArea.y, workArea.w, workArea.h},
        std::nullopt, 0, true);

    updateCanvasTranslation(packed);
  }

  SP<ITarget> getNextCandidate(SP<ITarget> old) override {
    const auto it =
        std::ranges::find_if(m_targets, [&](const auto &sizedTarget) {
          return sizedTarget.target.lock() == old;
        });
    if (it == m_targets.end())
      return nullptr;

    const auto previous =
        it == m_targets.begin() ? std::prev(m_targets.end()) : std::prev(it);
    return previous == it ? nullptr : previous->target.lock();
  }

  Config::ErrorResult layoutMsg(const std::string_view &message) override {
    if (message == "togglefocus") {
      m_focusMode = !m_focusMode;
      updateCanvasTranslation(m_memo.out);
    }
    return {};
  }

  std::optional<Vector2D> predictSizeForNewTarget() override {
    return std::nullopt;
  }

  void swapTargets(SP<ITarget> a, SP<ITarget> b) override {
    const auto first =
        std::ranges::find_if(m_targets, [&](const auto &sizedTarget) {
          return sizedTarget.target.lock() == a;
        });
    const auto second =
        std::ranges::find_if(m_targets, [&](const auto &sizedTarget) {
          return sizedTarget.target.lock() == b;
        });
    if (first != m_targets.end() && second != m_targets.end()) {
      std::iter_swap(first, second);
      recalculate();
    }
  }

  void moveTargetInDirection(SP<ITarget>, Math::eDirection, bool) override {}

private:
  struct SSizedTarget {
    WP<ITarget> target;
    Vector2D size;
  };

  void centerTarget(const SCanvasPack &packed, const SP<ITarget> &target) {
    const auto it =
        std::ranges::find_if(m_targets, [&](const auto &sizedTarget) {
          return sizedTarget.target.lock() == target;
        });
    if (it == m_targets.end())
      return;

    const auto index =
        static_cast<size_t>(std::distance(m_targets.begin(), it));
    const auto &rect = packed.rects[index];
    translateCanvas(packed,
                    Vector2D{rect.x + rect.w / 2.0, rect.y + rect.h / 2.0},
                    m_parent->space()->workArea().middle());
  }

  void updateCanvasTranslation(const SCanvasPack &packed) {
    if (m_focusMode) {
      const auto window = Desktop::focusState()->window();
      if (window)
        centerTarget(packed, window->layoutTarget());
    } else {
      const auto workArea = m_parent->space()->workArea();
      translateCanvas(packed, Vector2D{packed.w / 2.0, packed.h / 2.0},
                      workArea.middle());
    }
  }

  Vector2D defaultSize() const {
    return Hyprmosaic::Config::defaultSize(
        m_parent->space()->workArea().size());
  }

  // Translate canvasPoint in packer-local coordinates onto globalPoint.
  void translateCanvas(const SCanvasPack &packed, const Vector2D &canvasPoint,
                       const Vector2D &globalPoint) {
    const auto origin = globalPoint - canvasPoint;
    for (size_t i = 0; i < m_targets.size(); ++i) {
      const auto &rect = packed.rects[i];
      m_targets[i].target.lock()->setPositionGlobal(
          CBox{origin.x + rect.x, origin.y + rect.y, rect.w, rect.h});
    }
  }

  std::vector<SSizedTarget> m_targets;
  SCanvasMemo m_memo;
  bool m_focusMode;
  CHyprSignalListener m_focusListener;
};
} // namespace

UP<Layout::ITiledAlgorithm> Hyprmosaic::makeLayout() {
  return makeUnique<CMosaicLayout>();
}

const std::type_info *Hyprmosaic::layoutType() {
  return &typeid(CMosaicLayout);
}
