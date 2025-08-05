#!/usr/bin/env python3
import gi
gi.require_version("Gtk", "3.0")
from gi.repository import Gtk, Gdk, Pango, PangoCairo

import cairo
import base64
import serial
import serial.tools.list_ports

DISP_W_UNIT = 28
DISP_H = 19
SCALE = 10
CMD = 0x01
ADDRESSES = {0x08, 0x09}

class DisplayEditor(Gtk.Window):
    def __init__(self):
        super().__init__(title="Flippity210 Editor")
        self.set_resizable(False)

        self.screens = 5
        self.addr = 0x3C
        self.serial = None
        self.baudrate = 19200

        self.drawing = [[0 for _ in range(self.screens * DISP_W_UNIT)] for _ in range(DISP_H)]
        self.mouse_down = False
        self.mouse_erase = False
        self.inverse_text = False

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

        # === Controls Row 2: Clear/Fill/Export + Screens/Addr
        controls2 = Gtk.Box(spacing=6)
        btn_clear = Gtk.Button(label="Clear")
        btn_clear.connect("clicked", self.clear)
        btn_fill = Gtk.Button(label="Fill")
        btn_fill.connect("clicked", self.fill)
        btn_export = Gtk.Button(label="Export")
        btn_export.connect("clicked", self.export_frame)

        self.spin_screens = Gtk.SpinButton()
        self.spin_screens.set_range(1, 16)
        self.spin_screens.set_value(self.screens)
        self.spin_screens.connect("value-changed", self.change_screens)

        self.addr_combo = Gtk.ComboBoxText()
        for a in ADDRESSES:
            self.addr_combo.append_text(f"{a:02X}")
        self.addr_combo.set_active_id(f"{self.addr:02X}")
        self.addr_combo.connect("changed", self.change_address)

        controls2.pack_start(btn_clear, False, False, 0)
        controls2.pack_start(btn_fill, False, False, 0)
        controls2.pack_start(btn_export, False, False, 0)
        controls2.pack_start(Gtk.Label(label="Screens:"), False, False, 0)
        controls2.pack_start(self.spin_screens, False, False, 0)
        controls2.pack_start(Gtk.Label(label="Address:"), False, False, 0)
        controls2.pack_start(self.addr_combo, False, False, 0)

        main_vbox.pack_start(controls2, False, False, 0)

        # === Frame Output + Serial
        controls3 = Gtk.Box(spacing=6)
        self.output_entry = Gtk.Entry()
        self.output_entry.set_width_chars(90)

        self.port_combo = Gtk.ComboBoxText()
        for p in serial.tools.list_ports.comports():
            self.port_combo.append_text(p.device)
        self.port_combo.set_active(0)

        self.baud_spin = Gtk.SpinButton()
        self.baud_spin.set_range(1200, 115200)
        self.baud_spin.set_increments(100, 1000)
        self.baud_spin.set_value(self.baudrate)

        btn_send = Gtk.Button(label="Send")
        btn_send.connect("clicked", self.send_serial)

        btn_load = Gtk.Button(label="Load Frame")
        btn_load.connect("clicked", self.load_frame)

        controls3.pack_start(Gtk.Label(label="Frame:"), False, False, 0)
        controls3.pack_start(self.output_entry, False, False, 0)
        controls3.pack_start(self.port_combo, False, False, 0)
        controls3.pack_start(self.baud_spin, False, False, 0)
        controls3.pack_start(btn_send, False, False, 0)
        controls3.pack_start(btn_load, False, False, 0)

        main_vbox.pack_start(controls3, False, False, 0)

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

    def get_buffer(self):
        buf = bytearray()
        # Each display row padded to 32 bits = 4 bytes
        bytes_per_display_row = 4
        total_bytes_per_row = bytes_per_display_row * self.screens
        width = self.screens * DISP_W_UNIT

        for y in range(DISP_H):
            for screen in range(self.screens):
                x_offset = screen * DISP_W_UNIT
                byte = 0
                bit_index = 0
                for x in range(DISP_W_UNIT):
                    if self.drawing[y][x_offset + x]:
                        byte |= 1 << bit_index
                    bit_index += 1
                    if bit_index == 8:
                        buf.append(byte)
                        byte = 0
                        bit_index = 0
                # pad remaining bits in byte (unused, but must add full byte count)
                if bit_index > 0:
                    buf.append(byte)
        return buf


    def load_frame(self, *_):
        line = self.output_entry.get_text()
        if not line.startswith("!FRAME;"):
            return
        try:
            parts = line[7:].split(";")
            addr = int(parts[0].split("=")[1], 16)
            cmd = int(parts[1], 16)
            length = int(parts[2], 16)
            b64 = parts[3]
            raw = base64.b64decode(b64)
        except Exception as e:
            print("Parse error:", e)
            return

        self.addr = addr
        self.addr_combo.set_active_id(f"{addr:02X}")

        width = self.screens * DISP_W_UNIT
        self.drawing = [[0 for _ in range(width)] for _ in range(DISP_H)]

        i = 0
        bytes_per_display_row = 4
        for y in range(DISP_H):
            for screen in range(self.screens):
                for b in range(bytes_per_display_row):
                    if i >= len(raw):
                        break
                    byte = raw[i]
                    for bit in range(8):
                        x = screen * DISP_W_UNIT + b * 8 + bit
                        if x < width:
                            self.drawing[y][x] = (byte >> bit) & 1
                    i += 1
        self.canvas.queue_draw()


    def export_frame(self, *_):
        buf = self.get_buffer()
        b64 = base64.b64encode(buf).decode()
        header = f"!FRAME;ADDR={self.addr:02X};{CMD:02X};{len(buf):X};"
        frame = header + b64
        self.output_entry.set_text(frame)

    def change_screens(self, spin):
        self.screens = int(spin.get_value())
        self.drawing = [[0 for _ in range(self.screens * DISP_W_UNIT)] for _ in range(DISP_H)]
        self.update_canvas_size()
        self.canvas.queue_draw()

    def change_address(self, combo):
        try:
            self.addr = int(combo.get_active_text(), 16)
        except: pass

    def send_serial(self, *_):
        self.export_frame()
        port = self.port_combo.get_active_text()
        if not port: return
        try:
            baud = int(self.baud_spin.get_value())
            with serial.Serial(port, baud, timeout=1) as s:
                s.write((self.output_entry.get_text() + "\n").encode("ascii"))
        except Exception as e:
            print("Serial error:", e)

if __name__ == "__main__":
    win = DisplayEditor()
    win.connect("destroy", Gtk.main_quit)
    win.show_all()
    Gtk.main()

