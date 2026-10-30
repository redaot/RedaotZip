#!/usr/bin/env python3
"""Transforma un manual Markdown in PDF (A4), cu iconita RedaotZip in antet.
Utilizare: md2pdf.py <intrare.md> <iesire.pdf>   (necesita: pip install markdown; playwright pentru Python)"""
import base64, os, sys, markdown
from playwright.sync_api import sync_playwright

src, out = sys.argv[1], sys.argv[2]
root = os.path.dirname(os.path.dirname(os.path.abspath(__file__)))
icon = base64.b64encode(open(os.path.join(root, "assets", "icon512.png"), "rb").read()).decode()
body = markdown.markdown(open(src, encoding="utf-8").read(), extensions=["tables", "sane_lists"])
html = f"""<!doctype html><html><head><meta charset="utf-8"><style>
@page {{ size: A4; margin: 18mm 16mm 18mm 16mm; }}
body {{ font-family: "Segoe UI", "DejaVu Sans", Arial, sans-serif; font-size: 10.5pt; color: #1d2433; line-height: 1.45; }}
header {{ display: flex; align-items: center; gap: 14px; border-bottom: 2px solid #2f9e8f; padding-bottom: 10px; margin-bottom: 6px; }}
header img {{ width: 56px; height: 56px; }}
h1 {{ font-size: 22pt; margin: 0; color: #12403a; }}
h2 {{ font-size: 14pt; color: #12403a; border-bottom: 1px solid #d5e3e0; padding-bottom: 3px; margin-top: 22px; break-after: avoid; }}
table {{ border-collapse: collapse; width: 100%; margin: 8px 0 12px; font-size: 9.5pt; break-inside: auto; }}
th, td {{ border: 1px solid #cfd8d6; padding: 5px 7px; text-align: left; vertical-align: top; }}
th {{ background: #e8f3f1; }}
tr {{ break-inside: avoid; }}
code {{ background: #f1f4f4; padding: 1px 4px; border-radius: 3px; font-size: 9pt; }}
a {{ color: #1f7a6e; text-decoration: none; }}
</style></head><body>
<header><img src="data:image/png;base64,{icon}"><div>{body.split('</h1>',1)[0].replace('<h1>','<h1>')}</h1></div></header>
{body.split('</h1>',1)[1]}
</body></html>"""
with sync_playwright() as p:
    b = p.chromium.launch(); pg = b.new_page()
    pg.set_content(html, wait_until="load")
    pg.pdf(path=out, format="A4", print_background=True, display_header_footer=True,
           header_template="<span></span>",
           footer_template='<div style="font-size:8px;width:100%;text-align:center;color:#888">RedaotZip — made by REDAOT — <span class="pageNumber"></span>/<span class="totalPages"></span></div>',
           margin={"top": "16mm", "bottom": "16mm", "left": "14mm", "right": "14mm"})
    b.close()
print("PDF:", out)
