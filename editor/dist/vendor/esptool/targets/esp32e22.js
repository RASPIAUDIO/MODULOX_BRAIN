import { ESP32ROM } from "./esp32.js";
export class ESP32E22ROM extends ESP32ROM {
    constructor() {
        super(...arguments);
        this.CHIP_NAME = "ESP32-E22";
        this.IMAGE_CHIP_ID = 31;
        this.USES_MAGIC_VALUE = false;
        this.IROM_MAP_START = 0x3c000000;
        this.IROM_MAP_END = 0x40000000;
        this.DROM_MAP_START = 0x3c000000;
        this.DROM_MAP_END = 0x40000000;
        this.BOOTLOADER_FLASH_OFFSET = 0x0;
        this.UART_DATE_REG_ADDR = 0xc3102000 + 0x8c;
        this.UART_CLKDIV_REG = 0xc3102000 + 0x14;
        this.EFUSE_BASE = 0xc4008000;
        this.EFUSE_BLOCK1_ADDR = this.EFUSE_BASE + 0x044;
        this.MAC_EFUSE_REG = this.EFUSE_BASE + 0x044;
        this.SPI_REG_BASE = 0xc3003000;
        this.SPI_USR_OFFS = 0x18;
        this.SPI_USR1_OFFS = 0x1c;
        this.SPI_USR2_OFFS = 0x20;
        this.SPI_MOSI_DLEN_OFFS = 0x24;
        this.SPI_MISO_DLEN_OFFS = 0x28;
        this.SPI_W0_OFFS = 0x58;
        this.SPI_ADDR_REG_MSB = false;
        this.EFUSE_RD_REG_BASE = this.EFUSE_BASE + 0x030;
        this.EFUSE_PURPOSE_KEY0_REG = this.EFUSE_BASE + 0x34;
        this.EFUSE_PURPOSE_KEY0_SHIFT = 24;
        this.EFUSE_PURPOSE_KEY1_REG = this.EFUSE_BASE + 0x34;
        this.EFUSE_PURPOSE_KEY1_SHIFT = 28;
        this.EFUSE_PURPOSE_KEY2_REG = this.EFUSE_BASE + 0x38;
        this.EFUSE_PURPOSE_KEY2_SHIFT = 0;
        this.EFUSE_PURPOSE_KEY3_REG = this.EFUSE_BASE + 0x38;
        this.EFUSE_PURPOSE_KEY3_SHIFT = 4;
        this.EFUSE_PURPOSE_KEY4_REG = this.EFUSE_BASE + 0x38;
        this.EFUSE_PURPOSE_KEY4_SHIFT = 8;
        this.EFUSE_PURPOSE_KEY5_REG = this.EFUSE_BASE + 0x38;
        this.EFUSE_PURPOSE_KEY5_SHIFT = 12;
        this.EFUSE_SECURE_BOOT_EN_REG = this.EFUSE_BASE + 0x038;
        this.EFUSE_SECURE_BOOT_EN_MASK = 1 << 20;
        this.PURPOSE_VAL_XTS_AES256_KEY_1 = 2;
        this.PURPOSE_VAL_XTS_AES256_KEY_2 = 3;
        this.PURPOSE_VAL_XTS_AES128_KEY = 4;
        this.FLASH_ENCRYPTED_WRITE_ALIGN = 16;
        this.USB_RAM_BLOCK = 0x800;
        this.MEMORY_MAP = [
            [0x00000000, 0x00010000, "PADDING"],
            [0x3c000000, 0x40000000, "DROM"],
            [0x31000000, 0x31200000, "DRAM"],
            [0x31000000, 0x31200000, "BYTE_ACCESSIBLE"],
            [0x30000000, 0x30120000, "DROM_MASK"],
            [0x30000000, 0x30120000, "IROM_MASK"],
            [0x3c000000, 0x40000000, "IROM"],
            [0x30fe0000, 0x31200000, "IRAM"],
            [0xc0000000, 0xc0008000, "RTC_IRAM"],
            [0xc0000000, 0xc0008000, "RTC_DRAM"],
        ];
        this.EFUSE_MAX_KEY = 5;
        this.KEY_PURPOSES = {
            0: "USER/EMPTY",
            1: "ECDSA_KEY",
            2: "XTS_AES_256_KEY_1",
            3: "XTS_AES_256_KEY_2",
            4: "XTS_AES_128_KEY",
            5: "HMAC_DOWN_ALL",
            6: "HMAC_DOWN_JTAG",
            7: "HMAC_DOWN_DIGITAL_SIGNATURE",
            8: "HMAC_UP",
            9: "SECURE_BOOT_DIGEST0",
            10: "SECURE_BOOT_DIGEST1",
            11: "SECURE_BOOT_DIGEST2",
            12: "KM_INIT_KEY",
        };
    }
    async getPkgVersion(loader) {
        return 0;
    }
    async getMinorChipVersion(loader) {
        return 0;
    }
    async getMajorChipVersion(loader) {
        return 0;
    }
    async getChipRevision(loader) {
        const major = await this.getMajorChipVersion(loader);
        const minor = await this.getMinorChipVersion(loader);
        return major * 100 + minor;
    }
    async getChipDescription(loader) {
        const pkgVer = await this.getPkgVersion(loader);
        const desc = pkgVer === 0 ? "ESP32-E22" : "unknown ESP32-E22";
        const majorRev = await this.getMajorChipVersion(loader);
        const minorRev = await this.getMinorChipVersion(loader);
        return `${desc} (revision v${majorRev}.${minorRev})`;
    }
    async getChipFeatures(loader) {
        return ["Wi-Fi 6E (tri-band, 2x2 MU-MIMO)", "BT 5.4 (LE) + Classic", "Dual Core", "500MHz"];
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
    async getKeyBlockPurpose(loader, keyBlock) {
        if (keyBlock < 0 || keyBlock > this.EFUSE_MAX_KEY) {
            throw new Error(`Valid key block numbers must be in range 0-${this.EFUSE_MAX_KEY}`);
        }
        const regShiftDictionary = [
            [this.EFUSE_PURPOSE_KEY0_REG, this.EFUSE_PURPOSE_KEY0_SHIFT],
            [this.EFUSE_PURPOSE_KEY1_REG, this.EFUSE_PURPOSE_KEY1_SHIFT],
            [this.EFUSE_PURPOSE_KEY2_REG, this.EFUSE_PURPOSE_KEY2_SHIFT],
            [this.EFUSE_PURPOSE_KEY3_REG, this.EFUSE_PURPOSE_KEY3_SHIFT],
            [this.EFUSE_PURPOSE_KEY4_REG, this.EFUSE_PURPOSE_KEY4_SHIFT],
            [this.EFUSE_PURPOSE_KEY5_REG, this.EFUSE_PURPOSE_KEY5_SHIFT],
        ];
        const [reg, shift] = regShiftDictionary[keyBlock];
        const registerValue = await loader.readReg(reg);
        return (registerValue >> shift) & 0xf;
    }
    checkSpiConnection(loader, spiConnection) {
        if (!spiConnection.every((pin) => pin >= 0 && pin <= 52)) {
            throw new Error("SPI Pin numbers must be in the range 0-52.");
        }
        if (spiConnection.some((pin) => pin === 18 || pin === 19)) {
            loader.info("GPIO pins 18 and 19 are used by USB-OTG, " + "consider using other pins for SPI flash connection.");
        }
    }
    async postConnect(loader) {
        if (await loader.usesUsbOtg()) {
            loader.ESP_RAM_BLOCK = this.USB_RAM_BLOCK;
        }
    }
}
