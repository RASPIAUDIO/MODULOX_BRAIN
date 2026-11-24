import { ESP32FirmwareImage } from "./esp32.js";
export class ESP32S2FirmwareImage extends ESP32FirmwareImage {
    constructor(rom, loadFile = null, appendDigest = true, ramOnlyHeader = false) {
        super(rom, loadFile, appendDigest, ramOnlyHeader);
        this.ROM_LOADER = rom;
    }
}
export class ESP32S3FirmwareImage extends ESP32FirmwareImage {
    constructor(rom, loadFile = null, appendDigest = true, ramOnlyHeader = false) {
        super(rom, loadFile, appendDigest, ramOnlyHeader);
        this.ROM_LOADER = rom;
    }
}
export class ESP32C3FirmwareImage extends ESP32FirmwareImage {
    constructor(rom, loadFile = null, appendDigest = true, ramOnlyHeader = false) {
        super(rom, loadFile, appendDigest, ramOnlyHeader);
        this.ROM_LOADER = rom;
    }
}
export class ESP32C2FirmwareImage extends ESP32FirmwareImage {
    constructor(rom, loadFile = null, appendDigest = true, ramOnlyHeader = false) {
        super(rom, loadFile, appendDigest, ramOnlyHeader);
        this.MMU_PAGE_SIZE_CONF = [16384, 32768, 65536]; // 16KB, 32KB, 64KB
        this.ROM_LOADER = rom;
    }
}
export class ESP32C6FirmwareImage extends ESP32FirmwareImage {
    constructor(rom, loadFile = null, appendDigest = true, ramOnlyHeader = false) {
        super(rom, loadFile, appendDigest, ramOnlyHeader);
        this.MMU_PAGE_SIZE_CONF = [8192, 16384, 32768, 65536];
        this.ROM_LOADER = rom;
    }
}
export class ESP32C61FirmwareImage extends ESP32C6FirmwareImage {
    constructor(rom, loadFile = null, appendDigest = true, ramOnlyHeader = false) {
        super(rom, loadFile, appendDigest, ramOnlyHeader);
        this.ROM_LOADER = rom;
    }
}
export class ESP32C5FirmwareImage extends ESP32FirmwareImage {
    constructor(rom, loadFile = null, appendDigest = true, ramOnlyHeader = false) {
        super(rom, loadFile, appendDigest, ramOnlyHeader);
        this.ROM_LOADER = rom;
    }
}
export class ESP32P4FirmwareImage extends ESP32FirmwareImage {
    constructor(rom, loadFile = null, appendDigest = true, ramOnlyHeader = false) {
        super(rom, loadFile, appendDigest, ramOnlyHeader);
        this.ROM_LOADER = rom;
    }
}
export class ESP32H2FirmwareImage extends ESP32C6FirmwareImage {
    constructor(rom, loadFile = null, appendDigest = true, ramOnlyHeader = false) {
        super(rom, loadFile, appendDigest, ramOnlyHeader);
        this.ROM_LOADER = rom;
    }
}
export class ESP32H21FirmwareImage extends ESP32C6FirmwareImage {
    constructor(rom, loadFile = null, appendDigest = true, ramOnlyHeader = false) {
        super(rom, loadFile, appendDigest, ramOnlyHeader);
        this.ROM_LOADER = rom;
    }
}
export class ESP32H4FirmwareImage extends ESP32FirmwareImage {
    constructor(rom, loadFile = null, appendDigest = true, ramOnlyHeader = false) {
        super(rom, loadFile, appendDigest, ramOnlyHeader);
        this.MMU_PAGE_SIZE_CONF = [8192, 16384, 32768, 65536];
        this.ROM_LOADER = rom;
    }
}
export class ESP32S31FirmwareImage extends ESP32C5FirmwareImage {
    constructor(rom, loadFile = null, appendDigest = true, ramOnlyHeader = false) {
        super(rom, loadFile, appendDigest, ramOnlyHeader);
        this.MMU_PAGE_SIZE_CONF = [32768, 65536, 131072, 262144];
        this.ROM_LOADER = rom;
    }
}
export class ESP32E22FirmwareImage extends ESP32FirmwareImage {
    constructor(rom, loadFile = null, appendDigest = true, ramOnlyHeader = false) {
        super(rom, loadFile, appendDigest, ramOnlyHeader);
        this.ROM_LOADER = rom;
    }
}
