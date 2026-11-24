#pragma once

#include <hyprland/src/devices/IPointer.hpp>
#include <hyprland/src/event/EventBus.hpp>

#include <cstdint>
#include <optional>

namespace Hyprmosaic {
class ICanvasDragTarget {
public:
  virtual ~ICanvasDragTarget() = default;
  [[nodiscard]] virtual bool
  acceptsCanvasDrag(const Vector2D &position) const = 0;
  virtual void panCanvas(const Vector2D &delta) = 0;
};

class CCanvasDragController {
public:
  explicit CCanvasDragController(ICanvasDragTarget &target);
  void beginDrag();

private:
  ICanvasDragTarget &m_target;
  std::optional<std::uint32_t> m_pressedButton;
  std::optional<std::uint32_t> m_activeButton;
  Vector2D m_previousPointerPosition;
  CHyprSignalListener m_pointerMoveListener;
  CHyprSignalListener m_pointerButtonListener;
};
} // namespace Hyprmosaic
