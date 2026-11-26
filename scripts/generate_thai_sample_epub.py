#!/usr/bin/env python3
"""Generate test/epubs/thai_sample.epub for the simulator device check.

Original Thai text written for this project. It covers stacked marks
(upper vowel + tone mark), lower vowels, long runs without spaces for
line breaking, and mixed Thai/English with numbers.
"""

import zipfile
from html import escape
from pathlib import Path

OUTPUT = Path(__file__).resolve().parent.parent / "test" / "epubs" / "thai_sample.epub"

CHAPTERS = [
    (
        "บทที่ 1 ฝนแรกของเดือน",
        [
            "เช้าวันที่ 3 ของเดือนมิถุนายน ฝนตกลงมาตั้งแต่ตีห้า น้ำค้างบนใบไม้ยังไม่ทันแห้ง "
            "เด็กหญิงคนหนึ่งเปิดหน้าต่างแล้วสูดกลิ่นดินที่เปียกชื้นเข้าเต็มปอด",
            "เธอหยิบเครื่องอ่านหนังสือ e-reader ขนาด 4.3 นิ้วขึ้นมา เปิดหนังสือเล่มเดิมที่อ่านค้างไว้เมื่อคืน "
            "หน้าจอแสดงตัวอักษรคมชัดราวกับพิมพ์บนกระดาษจริง แบตเตอรี่ยังเหลืออยู่ถึง 87%",
            "คุณยายเดินถือถ้วยน้ำชาร้อนเข้ามาในห้อง พร้อมกับขนมกล้วยที่เพิ่งนึ่งเสร็จใหม่ๆ "
            "กลิ่นใบตองหอมฟุ้งไปทั่วบ้าน ทำให้ทุกคนที่นั่งอยู่ในห้องรู้สึกอุ่นใจขึ้นมาทันที",
            "ประโยคต่อไปนี้ไม่มีการเว้นวรรคเลยเพื่อทดสอบการตัดคำภาษาไทยว่าโปรแกรมสามารถแบ่งบรรทัดได้ถูกต้องตามพจนานุกรมหรือไม่โดยไม่ตัดกลางพยางค์และไม่ทำให้สระหรือวรรณยุกต์หลุดออกจากพยัญชนะ",
            "คำที่มีวรรณยุกต์ซ้อนกับสระบน เช่น ที่ นี่ ปี่ ชื่อ เสื้อ เปื้อน ผึ้ง กิ้งก่า "
            "และคำที่มีสระล่าง เช่น ครู ปู่ ดุ ฤดู ภูเขา ต้องแสดงตำแหน่งได้ถูกต้องไม่ทับกัน",
        ],
    ),
    (
        "บทที่ 2 ตลาดเช้า",
        [
            "ตลาดสดหน้าหมู่บ้านเปิดตั้งแต่ 05:30 น. แม่ค้าขายผักจัดวางคะน้า กะหล่ำปลี "
            "และพริกขี้หนูเป็นกองเล็กๆ ราคากองละ 10 บาท ส่วนปลาทูนึ่งขายเข่งละ 45 บาท",
            "ลุงขายกาแฟโบราณชงกาแฟด้วยถุงผ้าแบบดั้งเดิม เติมนมข้นหวานและน้ำแข็งจนเต็มแก้ว "
            "ลูกค้าประจำบางคนสั่งว่า Iced coffee, less sugar please แล้วหัวเราะกันทั้งร้าน",
            "เด็กนักเรียนสะพายกระเป๋าเดินผ่านไปพร้อมกับพูดคุยเรื่องการบ้านวิชาคณิตศาสตร์ข้อที่ 12 "
            "ซึ่งยากที่สุดในแบบฝึกหัดบทนี้ ทุกคนต่างเสนอวิธีคิดของตัวเองอย่างสนุกสนาน",
            "เมื่อแดดเริ่มแรงขึ้น ตลาดก็ค่อยๆ เงียบลง แม่ค้าเก็บแผงทีละร้าน "
            "เหลือไว้เพียงเสียงนกกระจอกที่บินลงมาจิกเศษข้าวบนพื้นถนน",
            "ตอนเย็นฝนก็ตกลงมาอีกครั้ง เด็กหญิงกลับมานั่งที่หน้าต่างบานเดิม "
            "เปิดหนังสือหน้าถัดไปแล้วอ่านต่อจนกระทั่งดวงอาทิตย์ลับขอบฟ้า",
        ],
    ),
]


def chapter_xhtml(title: str, paragraphs: list[str]) -> str:
    body = "\n".join(f"<p>{escape(p)}</p>" for p in paragraphs)
    return (
        '<?xml version="1.0" encoding="utf-8"?>\n'
        '<html xmlns="http://www.w3.org/1999/xhtml" xml:lang="th" lang="th">\n'
        f"<head><title>{escape(title)}</title></head>\n"
        f"<body>\n<h1>{escape(title)}</h1>\n{body}\n</body>\n</html>\n"
    )


def create_epub(path: Path) -> None:
    path.parent.mkdir(parents=True, exist_ok=True)
    manifest, spine, navpoints = [], [], []
    with zipfile.ZipFile(path, "w", zipfile.ZIP_DEFLATED) as epub:
        epub.writestr("mimetype", "application/epub+zip", compress_type=zipfile.ZIP_STORED)
        epub.writestr(
            "META-INF/container.xml",
            '<?xml version="1.0"?>\n'
            '<container version="1.0" xmlns="urn:oasis:names:tc:opendocument:xmlns:container">\n'
            '  <rootfiles><rootfile full-path="OEBPS/content.opf" '
            'media-type="application/oebps-package+xml"/></rootfiles>\n</container>\n',
        )
        for i, (title, paragraphs) in enumerate(CHAPTERS, start=1):
            cid = f"chapter{i}"
            epub.writestr(f"OEBPS/{cid}.xhtml", chapter_xhtml(title, paragraphs))
            manifest.append(f'<item id="{cid}" href="{cid}.xhtml" media-type="application/xhtml+xml"/>')
            spine.append(f'<itemref idref="{cid}"/>')
            navpoints.append(
                f'<navPoint id="nav{i}" playOrder="{i}"><navLabel><text>{escape(title)}</text></navLabel>'
                f'<content src="{cid}.xhtml"/></navPoint>'
            )
        epub.writestr(
            "OEBPS/content.opf",
            '<?xml version="1.0" encoding="utf-8"?>\n'
            '<package version="2.0" xmlns="http://www.idpf.org/2007/opf" unique-identifier="bookid">\n'
            '  <metadata xmlns:dc="http://purl.org/dc/elements/1.1/">\n'
            "    <dc:title>หนังสือตัวอย่างภาษาไทย</dc:title>\n"
            "    <dc:creator>CrossPoint Thai</dc:creator>\n"
            "    <dc:language>th</dc:language>\n"
            '    <dc:identifier id="bookid">crosspoint-thai-sample-001</dc:identifier>\n'
            "  </metadata>\n"
            f'  <manifest>{"".join(manifest)}<item id="ncx" href="toc.ncx" media-type="application/x-dtbncx+xml"/></manifest>\n'
            f'  <spine toc="ncx">{"".join(spine)}</spine>\n'
            "</package>\n",
        )
        epub.writestr(
            "OEBPS/toc.ncx",
            '<?xml version="1.0" encoding="utf-8"?>\n'
            '<ncx xmlns="http://www.daisy.org/z3986/2005/ncx/" version="2005-1">\n'
            '<head><meta name="dtb:uid" content="crosspoint-thai-sample-001"/></head>\n'
            "<docTitle><text>หนังสือตัวอย่างภาษาไทย</text></docTitle>\n"
            f'<navMap>{"".join(navpoints)}</navMap>\n</ncx>\n',
        )


if __name__ == "__main__":
    create_epub(OUTPUT)
    print(f"Wrote {OUTPUT}")
