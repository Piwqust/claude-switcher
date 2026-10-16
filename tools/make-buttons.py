"""Draws the README download buttons, docs/images/[ru/]download-{x64,arm64}.svg.

The text is turned into outlines with Segoe UI, so the buttons look the same on any
machine: GitHub shows README images as <img>, which cannot load fonts.
Needs fontTools and uharfbuzz (pip install fonttools uharfbuzz) and Windows' Segoe UI.

    python tools/make-buttons.py
"""
import os

import uharfbuzz as hb
from fontTools.pens.svgPathPen import SVGPathPen
from fontTools.pens.transformPen import TransformPen
from fontTools.ttLib import TTFont

HERE = os.path.dirname(os.path.abspath(__file__))
IMAGES = os.path.join(HERE, '..', 'docs', 'images')
FONTS = os.path.join(os.environ.get('WINDIR', r'C:\Windows'), 'Fonts')

COPY = {
    'en': {'title': 'Download for Windows', 'x64': 'x64 · Intel and AMD', 'arm64': 'ARM64 · Snapdragon'},
    'ru': {'title': 'Скачать для Windows', 'x64': 'x64 · Intel и AMD', 'arm64': 'ARM64 · Snapdragon'},
}
LOOKS = {
    'x64': {'fill': 'url(#bg)', 'gradient': ('#ea8a62', '#c9603b'), 'edge': 0.25,
            'title': '#ffffff', 'sub': '#ffffff', 'sub_opacity': 0.82, 'icon': '#ffffff'},
    'arm64': {'fill': '#21262d', 'gradient': None, 'edge': 0.16,
              'title': '#e6edf3', 'sub': '#9da7b3', 'sub_opacity': 1, 'icon': '#e6edf3'},
}
H, RADIUS, TEXT_X, PAD_RIGHT = 56, 14, 60, 26
TITLE_SIZE, SUB_SIZE = 17, 12.5


def number(v):
    return f'{v:.2f}'.rstrip('0').rstrip('.')


class Face:
    def __init__(self, file):
        path = os.path.join(FONTS, file)
        self.tt = TTFont(path)
        self.glyphs = self.tt.getGlyphSet()
        self.order = self.tt.getGlyphOrder()
        self.upem = self.tt['head'].unitsPerEm
        self.hb = hb.Font(hb.Face(hb.Blob.from_file_path(path)))

    def outline(self, text, size, x, y):
        """The path of `text` set at `size` px with its baseline at (x, y), and its width."""
        buf = hb.Buffer()
        buf.add_str(text)
        buf.guess_segment_properties()
        hb.shape(self.hb, buf, {'kern': True, 'liga': True})
        scale, pen_x, parts = size / self.upem, 0, []
        for info, pos in zip(buf.glyph_infos, buf.glyph_positions):
            pen = SVGPathPen(self.glyphs, ntos=number)
            offset_x = x + (pen_x + pos.x_offset) * scale
            offset_y = y - pos.y_offset * scale
            self.glyphs[self.order[info.codepoint]].draw(TransformPen(pen, (scale, 0, 0, -scale, offset_x, offset_y)))
            parts.append(pen.getCommands())
            pen_x += pos.x_advance
        return ''.join(parts), pen_x * scale


def button(bold, regular, title, sub, look, width):
    title_d, _ = bold.outline(title, TITLE_SIZE, TEXT_X, 25)
    sub_d, _ = regular.outline(sub, SUB_SIZE, TEXT_X, 43)
    defs = ''
    if look['gradient']:
        top, bottom = look['gradient']
        defs = (f'<defs><linearGradient id="bg" x1="0" y1="0" x2="0" y2="1">'
                f'<stop offset="0" stop-color="{top}"/><stop offset="1" stop-color="{bottom}"/></linearGradient></defs>')
    icon = ''.join(f'<rect x="{22 + dx}" y="{17 + dy}" width="10" height="10"/>' for dy in (0, 12) for dx in (0, 12))
    w = number(width)
    return (
        f'<svg xmlns="http://www.w3.org/2000/svg" width="{w}" height="{H}" viewBox="0 0 {w} {H}" role="img" aria-label="{title}, {sub}">'
        f'<title>{title}, {sub}</title>{defs}'
        f'<rect width="{w}" height="{H}" rx="{RADIUS}" fill="{look["fill"]}"/>'
        f'<rect x=".5" y=".5" width="{number(width - 1)}" height="{H - 1}" rx="{RADIUS - 0.5}" fill="none" stroke="#fff" stroke-opacity="{look["edge"]}"/>'
        f'<g fill="{look["icon"]}">{icon}</g>'
        f'<path fill="{look["title"]}" d="{title_d}"/>'
        f'<path fill="{look["sub"]}" fill-opacity="{look["sub_opacity"]}" d="{sub_d}"/>'
        f'</svg>\n'
    )


def main():
    bold, regular = Face('segoeuib.ttf'), Face('segoeui.ttf')
    for lang, copy in COPY.items():
        # Both buttons of a language share one width, so they line up side by side.
        widest = max([bold.outline(copy['title'], TITLE_SIZE, 0, 0)[1]] +
                     [regular.outline(copy[arch], SUB_SIZE, 0, 0)[1] for arch in LOOKS])
        width = round(TEXT_X + widest + PAD_RIGHT)
        dest = IMAGES if lang == 'en' else os.path.join(IMAGES, lang)
        for arch, look in LOOKS.items():
            path = os.path.normpath(os.path.join(dest, f'download-{arch}.svg'))
            with open(path, 'w', encoding='utf-8', newline='\n') as f:
                f.write(button(bold, regular, copy['title'], copy[arch], look, width))
            print(f'Wrote {path} ({width}x{H}, {os.path.getsize(path) // 1024} KB)')


if __name__ == '__main__':
    main()
