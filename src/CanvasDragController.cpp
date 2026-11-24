#include "CanvasDragController.hpp"

#include <hyprland/src/managers/input/InputManager.hpp>

Hyprmosaic::CCanvasDragController::CCanvasDragController(
    ICanvasDragTarget &target)
    : m_target(target) {
  m_pointerMoveListener = Event::bus()->m_events.input.mouse.move.listen(
      [this](Vector2D position, Event::SCallbackInfo &info) {
        if (!m_activeButton)
          return;

        m_target.panCanvas(position - m_previousPointerPosition);
        m_previousPointerPosition = position;
        info.cancelled = true;
      });
  m_pointerButtonListener = Event::bus()->m_events.input.mouse.button.listen(
      [this](IPointer::SButtonEvent event, Event::SCallbackInfo &) {
        const bool pressed = event.state == WL_POINTER_BUTTON_STATE_PRESSED;
        if (pressed && m_target.acceptsCanvasDrag(
                           g_pInputManager->getMouseCoordsInternal()))
          m_pressedButton = event.button;
        else if (!pressed) {
          if (m_activeButton == event.button)
            m_activeButton.reset();
          if (m_pressedButton == event.button)
            m_pressedButton.reset();
        }
      });
}

void Hyprmosaic::CCanvasDragController::beginDrag() {
  const auto position = g_pInputManager->getMouseCoordsInternal();
  if (!m_pressedButton || !m_target.acceptsCanvasDrag(position) ||
      g_pInputManager->isConstrained())
    return;

  m_activeButton = m_pressedButton;
  m_previousPointerPosition = position;
}
