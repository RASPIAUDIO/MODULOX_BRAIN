import { bstrToUi8, ESP_CHECKSUM_MAGIC } from "../util.js";
import { alignFilePosition, BaseFirmwareImage, ELFSection, ESP_IMAGE_MAGIC, ImageSegment } from "./base.js";
import { ESPError } from "../types/error.js";
export class ESP32FirmwareImage extends BaseFirmwareImage {
    constructor(rom, loadFile = null, appendDigest = true, ramOnlyHeader = false) {
        super(rom);
        this.securePad = null;
        this.flashMode = 0;
        this.flashSizeFreq = 0;
        this.version = 1;
        // ROM bootloader will read the wp_pin field if SPI flash
        // pins are remapped via flash. IDF actually enables QIO only
        // from software bootloader, so this can be ignored. But needs
        // to be set to this value so ROM bootloader will skip it.
        this.WP_PIN_DISABLED = 0xee;
        this.wpPin = this.WP_PIN_DISABLED;
        // Extended header fields
        this.clkDrv = 0;
        this.qDrv = 0;
        this.dDrv = 0;
        this.csDrv = 0;
        this.hdDrv = 0;
        this.wpDrv = 0;
        this.chipId = 0;
        this.minRev = 0;
        this.minRevFull = 0;
        this.maxRevFull = 0;
        this.storedDigest = null;
        this.calcDigest = null;
        this.dataLength = 0;
        this.IROM_ALIGN = 65536;
        this.ROM_LOADER = rom;
        this.appendDigest = appendDigest;
        this.ramOnlyHeader = ramOnlyHeader;
        if (loadFile !== null) {
            this.loadFromFile(loadFile);
        }
    }
    async loadFromFile(loadFile) {
        const start = 0;
        const binaryData = loadFile instanceof Uint8Array ? loadFile : bstrToUi8(loadFile);
        let offset = 0;
        const segments = this.loadCommonHeader(binaryData, offset, ESP_IMAGE_MAGIC);
        offset += 8;
        this.loadExtendedHeader(binaryData, offset);
        offset += 16; // Extended header is 16 bytes
        // Load segments
        for (let i = 0; i < segments; i++) {
            const segment = this.loadSegment(binaryData, offset);
            offset += 8 + segment.data.length;
        }
        // Read checksum
        this.checksum = this.readChecksum(binaryData, offset);
        offset = alignFilePosition(offset, 16);
        if (this.appendDigest) {
            const end = offset;
            this.storedDigest = binaryData.slice(offset, offset + this.SHA256_DIGEST_LEN);
            const shaDigest = await crypto.subtle.digest("SHA-256", binaryData.slice(start, end));
            this.calcDigest = new Uint8Array(shaDigest);
            this.dataLength = end - start;
        }
        this.verify();
    }
    isFlashAddr(addr) {
        return ((this.ROM_LOADER.IROM_MAP_START <= addr && addr < this.ROM_LOADER.IROM_MAP_END) ||
            (this.ROM_LOADER.DROM_MAP_START <= addr && addr < this.ROM_LOADER.DROM_MAP_END));
    }
    async save() {
        let totalSegments = 0;
        const output = new Uint8Array(1024 * 1024); // Start with 1MB buffer, will grow if needed
        let offset = 0;
        // Write common header
        this.writeCommonHeader(output, offset, this.segments.length);
        offset += 8;
        // Write extended header
        this.saveExtendedHeader(output, offset);
        offset += 16;
        let checksum = ESP_CHECKSUM_MAGIC;
        // Split segments into flash-mapped vs ram-loaded
        const flashSegments = this.segments.filter((s) => this.isFlashAddr(s.addr)).sort((a, b) => a.addr - b.addr);
        const ramSegments = this.segments.filter((s) => !this.isFlashAddr(s.addr)).sort((a, b) => a.addr - b.addr);
        // Patch to support ESP32-C6 union bus memmap
        // move ".flash.appdesc" segment to the top of the flash segment
        for (let i = 0; i < flashSegments.length; i++) {
            const segment = flashSegments[i];
            if (segment instanceof ELFSection && segment.name === ".flash.appdesc") {
                flashSegments.splice(i, 1);
                flashSegments.unshift(segment);
                break;
            }
        }
        // For the bootloader image
        // move ".dram0.bootdesc" segment to the top of the ram segment
        // So bootdesc will be at the very top of the binary at 0x20 offset
        // (in the first segment).
        for (let i = 0; i < ramSegments.length; i++) {
            const segment = ramSegments[i];
            if (segment instanceof ELFSection && segment.name === ".dram0.bootdesc") {
                ramSegments.splice(i, 1);
                ramSegments.unshift(segment);
                break;
            }
        }
        // Check for multiple ELF sections in same flash mapping region
        if (flashSegments.length > 0) {
            let lastAddr = flashSegments[0].addr;
            for (const segment of flashSegments.slice(1)) {
                if (Math.floor(segment.addr / this.IROM_ALIGN) === Math.floor(lastAddr / this.IROM_ALIGN)) {
                    throw new ESPError(`Segment loaded at 0x${segment.addr.toString(16)} lands in same 64KB flash mapping ` +
                        `as segment loaded at 0x${lastAddr.toString(16)}. Can't generate binary. ` +
                        "Suggest changing linker script or ELF to merge sections.");
                }
                lastAddr = segment.addr;
            }
        }
        if (this.ramOnlyHeader) {
            // Write RAM segments first
            for (const segment of ramSegments) {
                checksum = this.saveSegment(output, offset, segment, checksum);
                offset += 8 + segment.data.length;
                totalSegments++;
            }
            this.appendChecksum(output, offset, checksum);
            offset = alignFilePosition(offset, 16);
            // Write flash segments
            for (const segment of flashSegments.reverse()) {
                let padLen = this.getAlignmentDataNeeded(segment, offset);
                if (padLen > 0) {
                    const align_min = this.ROM_LOADER.BOOTLOADER_FLASH_OFFSET - this.SEG_HEADER_LEN;
                    if (padLen < align_min) {
                        // in case pad_len does not fit minimum alignment,
                        // pad it to next aligned boundary
                        padLen += this.IROM_ALIGN;
                    }
                    padLen -= this.ROM_LOADER.BOOTLOADER_FLASH_OFFSET;
                    const padSegment = new ImageSegment(0, new Uint8Array(padLen).fill(0), offset);
                    checksum = this.saveSegment(output, offset, padSegment, checksum);
                    offset += 8 + padLen;
                    totalSegments++;
                }
                this.saveFlashSegment(output, offset, segment);
                offset += 8 + segment.data.length;
                totalSegments++;
            }
        }
        else {
            // Write flash segments with padding
            while (flashSegments.length > 0) {
                const segment = flashSegments[0];
                const padLen = this.getAlignmentDataNeeded(segment, offset);
                if (padLen > 0) {
                    // need to pad
                    if (ramSegments.length > 0 && padLen > this.SEG_HEADER_LEN) {
                        // Split a part of the first RAM segment to use as padding
                        const padSegment = ramSegments[0].splitImage(padLen);
                        if (ramSegments[0].data.length === 0) {
                            ramSegments.shift();
                        }
                        checksum = this.saveSegment(output, offset, padSegment, checksum);
                    }
                    else {
                        // Use zero padding
                        const padSegment = new ImageSegment(0, new Uint8Array(padLen).fill(0), offset);
                        checksum = this.saveSegment(output, offset, padSegment, checksum);
                    }
                    offset += 8 + padLen;
                    totalSegments++;
                }
                else {
                    // write the flash segment
                    if ((offset + 8) % this.IROM_ALIGN !== segment.addr % this.IROM_ALIGN) {
                        throw new Error("Flash segment alignment mismatch");
                    }
                    checksum = this.saveFlashSegment(output, offset, segment, checksum);
                    flashSegments.shift();
                    offset += 8 + segment.data.length;
                    totalSegments++;
                }
            }
            // Write remaining RAM segments
            for (const segment of ramSegments) {
                checksum = this.saveSegment(output, offset, segment, checksum);
                offset += 8 + segment.data.length;
                totalSegments++;
            }
        }
        // Handle secure padding if needed
        if (this.securePad) {
            if (!this.appendDigest) {
                throw new Error("secure_pad only applies if a SHA-256 digest is also appended to the image");
            }
            const alignPast = (offset + this.SEG_HEADER_LEN) % this.IROM_ALIGN;
            const checksumSpace = 16; // 16 byte aligned checksum
            let spaceAfterChecksum = 0;
            if (this.securePad === "1") {
                // Secure Boot V1: SHA-256 digest + version + signature + 12 trailing bytes
                spaceAfterChecksum = 32 + 4 + 64 + 12;
            }
            else if (this.securePad === "2") {
                // Secure Boot V2: SHA-256 digest + signature sector (placed after 64KB boundary)
                spaceAfterChecksum = 32;
            }
            const padLen = (this.IROM_ALIGN - alignPast - checksumSpace - spaceAfterChecksum) % this.IROM_ALIGN;
            const padSegment = new ImageSegment(0, new Uint8Array(padLen).fill(0), offset);
            checksum = this.saveSegment(output, offset, padSegment, checksum);
            offset += 8 + padLen;
            totalSegments++;
        }
        // Append checksum after all segments are written
        if (!this.ramOnlyHeader) {
            this.appendChecksum(output, offset, checksum);
            offset = alignFilePosition(offset, 16);
        }
        const imageLength = offset;
        // Go back to the initial header and write the new segment count
        // This header is not checksummed
        if (this.ramOnlyHeader) {
            // Update the header with the RAM segments quantity as it should be
            // visible by the ROM bootloader
            output[1] = ramSegments.length;
        }
        else {
            output[1] = totalSegments;
        }
        if (this.appendDigest) {
            // calculate the SHA256 of the whole file and append it
            const shaDigest = await crypto.subtle.digest("SHA-256", output.slice(0, imageLength));
            const digest = new Uint8Array(shaDigest);
            output.set(digest, imageLength);
            offset += 32;
        }
        if (this.padToSize) {
            if (offset % this.padToSize !== 0) {
                const padBy = this.padToSize - (offset % this.padToSize);
                const padding = new Uint8Array(padBy);
                padding.fill(0xff);
                output.set(padding, offset);
                offset += padBy;
            }
        }
        return output;
    }
    loadExtendedHeader(data, offset) {
        const view = new DataView(data.buffer, offset);
        this.wpPin = view.getUint8(0);
        const driveConfig = view.getUint8(1);
        [this.clkDrv, this.qDrv] = this.splitByte(driveConfig);
        const dConfig = view.getUint8(2);
        [this.dDrv, this.csDrv] = this.splitByte(dConfig);
        const hdConfig = view.getUint8(3);
        [this.hdDrv, this.wpDrv] = this.splitByte(hdConfig);
        this.chipId = view.getUint8(4);
        if (this.ROM_LOADER.IMAGE_CHIP_ID !== undefined && this.chipId !== this.ROM_LOADER.IMAGE_CHIP_ID) {
            console.warn(`Unexpected chip id in image. Expected ${this.ROM_LOADER.IMAGE_CHIP_ID} but value was ${this.chipId}. ` +
                "Is this image for a different chip model?");
        }
        this.minRev = view.getUint8(5);
        this.minRevFull = view.getUint16(6, true);
        this.maxRevFull = view.getUint16(8, true);
        // Last byte is append_digest validation
        const appendDigest = view.getUint8(15);
        if (appendDigest === 0 || appendDigest === 1) {
            this.appendDigest = appendDigest === 1;
        }
        else {
            throw new Error(`Invalid value for append_digest field (0x${appendDigest.toString(16)}). Should be 0 or 1.`);
        }
    }
    saveExtendedHeader(output, offset) {
        var _a;
        const headerBuffer = new ArrayBuffer(16);
        const view = new DataView(headerBuffer);
        view.setUint8(0, this.wpPin);
        view.setUint8(1, this.joinByte(this.clkDrv, this.qDrv));
        view.setUint8(2, this.joinByte(this.dDrv, this.csDrv));
        view.setUint8(3, this.joinByte(this.hdDrv, this.wpDrv));
        view.setUint8(4, (_a = this.ROM_LOADER.IMAGE_CHIP_ID) !== null && _a !== void 0 ? _a : 0);
        view.setUint8(5, this.minRev);
        view.setUint16(6, this.minRevFull, true);
        view.setUint16(8, this.maxRevFull, true);
        for (let i = 9; i < 15; i++) {
            view.setUint8(i, 0);
        }
        view.setUint8(15, this.appendDigest ? 1 : 0);
        // Copy the header buffer to the output
        output.set(new Uint8Array(headerBuffer), offset);
    }
    splitByte(n) {
        return [n & 0x0f, (n >> 4) & 0x0f];
    }
    joinByte(ln, hn) {
        return (ln & 0x0f) | ((hn & 0x0f) << 4);
    }
    getAlignmentDataNeeded(segment, currentOffset) {
        // Calculate alignment needed for segment
        const alignPast = (segment.addr % this.IROM_ALIGN) - this.SEG_HEADER_LEN;
        let padLen = this.IROM_ALIGN - (currentOffset % this.IROM_ALIGN) + alignPast;
        if (padLen === 0 || padLen === this.IROM_ALIGN) {
            return 0; // already aligned
        }
        padLen -= this.SEG_HEADER_LEN;
        if (padLen < 0) {
            padLen += this.IROM_ALIGN;
        }
        return padLen;
    }
}
