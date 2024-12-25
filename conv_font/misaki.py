from pathlib import Path
from PIL import Image

SRCDIR = Path(__file__).parent


def main():
    im = Image.open(SRCDIR / "data" / "misaki_png_2021-05-05a")


if __name__ == "__main__":
    main()
