#!/usr/bin/env python3
import gi
gi.require_version("Gtk", "3.0")
from gi.repository import Gtk, Gdk, Pango, PangoCairo

import cairo
import base64
import io

DISP_W_UNIT = 28
DISP_H = 19
SCALE = 10

class DisplayEditor(Gtk.Window):
    def __init__(self):
        super().__init__(title="Flippity210 Editor")
        self.set_resizable(False)

        self.screens = 5
        self.drawing = [[0 for _ in range(self.screens * DISP_W_UNIT)] for _ in range(DISP_H)]
        self.mouse_down = False
        self.mouse_erase = False

        self.setup_ui()

    def setup_ui(self):
        main_vbox = Gtk.Box(orientation=Gtk.Orientation.VERTICAL, spacing=6)
        self.add(main_vbox)

        # === Drawing area
        self.canvas = Gtk.DrawingArea()
        self.update_canvas_size()
        self.canvas.connect("draw", self.on_draw)
        self.canvas.add_events(Gdk.EventMask.BUTTON_PRESS_MASK |
                               Gdk.EventMask.BUTTON_RELEASE_MASK |
                               Gdk.EventMask.POINTER_MOTION_MASK)
        self.canvas.connect("button-press-event", self.on_mouse_down)
        self.canvas.connect("button-release-event", self.on_mouse_up)
        self.canvas.connect("motion-notify-event", self.on_mouse_move)
        main_vbox.pack_start(self.canvas, False, False, 0)

        # === Controls Row 1: Text + Position + Font
        controls1 = Gtk.Box(spacing=6)
        self.entry_text = Gtk.Entry()
        self.entry_text.set_placeholder_text("Text")
        self.entry_text.set_width_chars(12)
        self.entry_pos = Gtk.Entry()
        self.entry_pos.set_placeholder_text("x,y")
        self.entry_pos.set_text("5,0")
        self.font_combo = Gtk.FontButton(title="Font")
        self.bold_check = Gtk.CheckButton(label="Bold")
        self.italic_check = Gtk.CheckButton(label="Italic")
        self.inverse_check = Gtk.CheckButton(label="Inverse")
        btn_draw = Gtk.Button(label="Draw Text")
        btn_draw.connect("clicked", self.draw_text)

        controls1.pack_start(Gtk.Label(label="Text:"), False, False, 0)
        controls1.pack_start(self.entry_text, False, False, 0)
        controls1.pack_start(Gtk.Label(label="Pos:"), False, False, 0)
        controls1.pack_start(self.entry_pos, False, False, 0)
        controls1.pack_start(self.font_combo, False, False, 0)
        controls1.pack_start(self.bold_check, False, False, 0)
        controls1.pack_start(self.italic_check, False, False, 0)
        controls1.pack_start(self.inverse_check, False, False, 0)
        controls1.pack_start(btn_draw, False, False, 0)

        main_vbox.pack_start(controls1, False, False, 0)

        # === Controls Row 2: Clear/Fill/Export/Load + Screens
        controls2 = Gtk.Box(spacing=6)
        btn_clear = Gtk.Button(label="Clear")
        btn_clear.connect("clicked", self.clear)
        btn_fill = Gtk.Button(label="Fill")
        btn_fill.connect("clicked", self.fill)
        btn_export = Gtk.Button(label="Export Base64 (PNG)")
        btn_export.connect("clicked", self.export_base64)
        btn_load = Gtk.Button(label="Load Base64")
        btn_load.connect("clicked", self.load_base64)

        self.spin_screens = Gtk.SpinButton()
        self.spin_screens.set_range(1, 16)
        self.spin_screens.set_value(self.screens)
        self.spin_screens.connect("value-changed", self.change_screens)

        controls2.pack_start(btn_clear, False, False, 0)
        controls2.pack_start(btn_fill, False, False, 0)
        controls2.pack_start(btn_export, False, False, 0)
        controls2.pack_start(btn_load, False, False, 0)
        controls2.pack_start(Gtk.Label(label="Screens:"), False, False, 0)
        controls2.pack_start(self.spin_screens, False, False, 0)

        main_vbox.pack_start(controls2, False, False, 0)

        # === Base64 Output/Input
        self.output_entry = Gtk.Entry()
        self.output_entry.set_width_chars(90)
        main_vbox.pack_start(self.output_entry, False, False, 0)

    def update_canvas_size(self):
        self.canvas.set_size_request(self.screens * DISP_W_UNIT * SCALE, DISP_H * SCALE)

    def on_draw(self, widget, cr):
        for y in range(DISP_H):
            for x in range(self.screens * DISP_W_UNIT):
                val = self.drawing[y][x]
                cr.set_source_rgb(1, 1, 1) if val else cr.set_source_rgb(0.2, 0.2, 0.2)
                size = SCALE * 0.75
                offset = (SCALE - size) / 2
                cr.rectangle(x * SCALE + offset, y * SCALE + offset, size, size)
                cr.fill()

    def clear(self, *_):
        self.drawing = [[0 for _ in range(self.screens * DISP_W_UNIT)] for _ in range(DISP_H)]
        self.canvas.queue_draw()

    def fill(self, *_):
        self.drawing = [[1 for _ in range(self.screens * DISP_W_UNIT)] for _ in range(DISP_H)]
        self.canvas.queue_draw()

    def on_mouse_down(self, widget, event):
        x, y = int(event.x // SCALE), int(event.y // SCALE)
        if 0 <= x < self.screens * DISP_W_UNIT and 0 <= y < DISP_H:
            self.mouse_down = True
            self.mouse_erase = (event.button == 3)
            self.drawing[y][x] = 0 if self.mouse_erase else 1
            self.canvas.queue_draw()

    def on_mouse_up(self, widget, event):
        self.mouse_down = False

    def on_mouse_move(self, widget, event):
        if self.mouse_down:
            x, y = int(event.x // SCALE), int(event.y // SCALE)
            if 0 <= x < self.screens * DISP_W_UNIT and 0 <= y < DISP_H:
                self.drawing[y][x] = 0 if self.mouse_erase else 1
                self.canvas.queue_draw()

    def draw_text(self, *_):
        text = self.entry_text.get_text()
        try:
            x_str, y_str = self.entry_pos.get_text().split(",")
            x0, y0 = int(x_str), int(y_str)
        except:
            return

        font_desc = self.font_combo.get_font_name()
        if self.bold_check.get_active():
            font_desc += " Bold"
        if self.italic_check.get_active():
            font_desc += " Italic"

        surface = cairo.ImageSurface(cairo.FORMAT_A8, self.screens * DISP_W_UNIT, DISP_H)
        ctx = cairo.Context(surface)

        layout = PangoCairo.create_layout(ctx)
        pfd = Pango.FontDescription(font_desc)
        layout.set_font_description(pfd)
        layout.set_text(text, -1)

        ctx.move_to(x0, y0)
        ctx.set_source_rgb(1, 1, 1)
        PangoCairo.show_layout(ctx, layout)

        data = surface.get_data()
        stride = surface.get_stride()
        for y in range(DISP_H):
            for x in range(self.screens * DISP_W_UNIT):
                pixel = data[y * stride + x]
                if pixel > 100:
                    self.drawing[y][x] = 0 if self.inverse_check.get_active() else 1

        self.canvas.queue_draw()

    def export_base64(self, *_):
        # Render to PNG Base64 (same format as web)
        width = self.screens * DISP_W_UNIT
        surface = cairo.ImageSurface(cairo.FORMAT_RGB24, width, DISP_H)
        ctx = cairo.Context(surface)

        # Draw pixels
        for y in range(DISP_H):
            for x in range(width):
                ctx.set_source_rgb(0, 0, 0) if self.drawing[y][x] else ctx.set_source_rgb(1, 1, 1)
                ctx.rectangle(x, y, 1, 1)
                ctx.fill()

        buffer = io.BytesIO()
        surface.write_to_png(buffer)
        b64 = base64.b64encode(buffer.getvalue()).decode()
        self.output_entry.set_text(b64)

    def load_base64(self, *_):
        # Load Base64 PNG into buffer
        b64_data = self.output_entry.get_text().strip()
        if not b64_data:
            return

        try:
            png_data = base64.b64decode(b64_data)
            surface = cairo.ImageSurface.create_from_png(io.BytesIO(png_data))
            ctx = cairo.Context(surface)

            width = self.screens * DISP_W_UNIT
            img_data = surface.get_data()
            stride = surface.get_stride()

            for y in range(DISP_H):
                for x in range(width):
                    # RGB24 format: 4 bytes per pixel (B,G,R,unused) but Cairo uses native endianness
                    pixel_offset = y * stride + x * 4
                    r = img_data[pixel_offset + 2]
                    g = img_data[pixel_offset + 1]
                    b = img_data[pixel_offset + 0]
                    # consider black if RGB average < 128
                    self.drawing[y][x] = 1 if (r + g + b) / 3 < 128 else 0

            self.canvas.queue_draw()

        except Exception as e:
            print("Failed to load Base64 PNG:", e)

    def change_screens(self, spin):
        self.screens = int(spin.get_value())
        self.drawing = [[0 for _ in range(self.screens * DISP_W_UNIT)] for _ in range(DISP_H)]
        self.update_canvas_size()
        self.canvas.queue_draw()


if __name__ == "__main__":
    win = DisplayEditor()
    win.connect("destroy", Gtk.main_quit)
    win.show_all()
    Gtk.main()

