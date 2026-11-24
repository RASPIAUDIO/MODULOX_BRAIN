import { ESP32C6ROM } from "./esp32c6.js";
export class ESP32H2ROM extends ESP32C6ROM {
    constructor() {
        super(...arguments);
        this.CHIP_NAME = "ESP32-H2";
        this.IMAGE_CHIP_ID = 16;
        this.EFUSE_BASE = 0x600b0800;
        this.EFUSE_BLOCK1_ADDR = this.EFUSE_BASE + 0x044;
        this.MAC_EFUSE_REG = this.EFUSE_BASE + 0x044;
        this.UART_CLKDIV_REG = 0x3ff40014;
        this.UART_CLKDIV_MASK = 0xfffff;
        this.UART_DATE_REG_ADDR = 0x6000007c;
        this.FLASH_WRITE_SIZE = 0x400;
        this.BOOTLOADER_FLASH_OFFSET = 0x0;
        this.SPI_REG_BASE = 0x60003000;
        this.SPI_USR_OFFS = 0x18;
        this.SPI_USR1_OFFS = 0x1c;
        this.SPI_USR2_OFFS = 0x20;
        this.SPI_MOSI_DLEN_OFFS = 0x24;
        this.SPI_MISO_DLEN_OFFS = 0x28;
        this.SPI_W0_OFFS = 0x58;
        this.USB_RAM_BLOCK = 0x800;
        this.UARTDEV_BUF_NO_USB = 3;
        this.UARTDEV_BUF_NO = 0x3fcef14c;
        this.IROM_MAP_START = 0x42000000;
        this.IROM_MAP_END = 0x42800000;
        this.MEMORY_MAP = [
            [0x00000000, 0x00010000, "PADDING"],
            [0x42000000, 0x43000000, "DROM"],
            [0x40800000, 0x40880000, "DRAM"],
            [0x40800000, 0x40880000, "BYTE_ACCESSIBLE"],
            [0x4004ac00, 0x40050000, "DROM_MASK"],
            [0x40000000, 0x4004ac00, "IROM_MASK"],
            [0x42000000, 0x43000000, "IROM"],
            [0x40800000, 0x40880000, "IRAM"],
            [0x50000000, 0x50004000, "RTC_IRAM"],
            [0x50000000, 0x50004000, "RTC_DRAM"],
            [0x600fe000, 0x60100000, "MEM_INTERNAL2"],
        ];
    }
    async getPkgVersion(loader) {
        const numWord = 4;
        return ((await loader.readReg(this.EFUSE_BLOCK1_ADDR + 4 * numWord)) >> 0) & 0x07;
    }
    async getMinorChipVersion(loader) {
        const numWord = 3;
        return ((await loader.readReg(this.EFUSE_BLOCK1_ADDR + 4 * numWord)) >> 18) & 0x07;
    }
    async getMajorChipVersion(loader) {
        const numWord = 3;
        return ((await loader.readReg(this.EFUSE_BLOCK1_ADDR + 4 * numWord)) >> 21) & 0x03;
    }
    async getChipDescription(loader) {
        const pkgVer = await this.getPkgVersion(loader);
        let desc;
        if (pkgVer === 0) {
            desc = "ESP32-H2";
        }
        else {
            desc = "unknown ESP32-H2";
        }
        const majorRev = await this.getMajorChipVersion(loader);
        const minorRev = await this.getMinorChipVersion(loader);
        return `${desc} (revision v${majorRev}.${minorRev})`;
    }
    async getChipFeatures(loader) {
        return ["BT 5 (LE)", "IEEE802.15.4", "Single Core", "96MHz"];
    }
    async getCrystalFreq(loader) {
        // ESP32H2 XTAL is fixed to 32MHz
        return 32;
    }
    _d2h(d) {
        const h = (+d).toString(16);
        return h.length === 1 ? "0" + h : h;
    }
    async postConnect(loader) {
        const bufNo = (await loader.readReg(this.UARTDEV_BUF_NO)) & 0xff;
        loader.debug("In _post_connect " + bufNo);
        if (bufNo == this.UARTDEV_BUF_NO_USB) {
            loader.ESP_RAM_BLOCK = this.USB_RAM_BLOCK;
        }
    }
    async usesUsbJtagSerial(loader) {
        const bufNo = (await loader.readReg(this.UARTDEV_BUF_NO)) & 0xff;
        return bufNo === this.UARTDEV_BUF_NO_USB;
    }
    async readMac(loader) {
        let mac0 = await loader.readReg(this.MAC_EFUSE_REG);
        mac0 = mac0 >>> 0;
        let mac1 = await loader.readReg(this.MAC_EFUSE_REG + 4);
        mac1 = (mac1 >>> 0) & 0x0000ffff;
        const mac = new Uint8Array(6);
        mac[0] = (mac1 >> 8) & 0xff;
        mac[1] = mac1 & 0xff;
        mac[2] = (mac0 >> 24) & 0xff;
        mac[3] = (mac0 >> 16) & 0xff;
        mac[4] = (mac0 >> 8) & 0xff;
        mac[5] = mac0 & 0xff;
        return (this._d2h(mac[0]) +
            ":" +
            this._d2h(mac[1]) +
            ":" +
            this._d2h(mac[2]) +
            ":" +
            this._d2h(mac[3]) +
            ":" +
            this._d2h(mac[4]) +
            ":" +
            this._d2h(mac[5]));
    }
    getEraseSize(offset, size) {
        return size;
    }
}
