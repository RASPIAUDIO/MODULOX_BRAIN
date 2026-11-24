import { ESP8266ROM } from "../targets/esp8266.js";
import { ESPError } from "../types/error.js";
import { checksum, ESP_CHECKSUM_MAGIC, padTo } from "../util.js";
export const ESP_IMAGE_MAGIC = 0xe9;
/**
 * Return position aligned to size
 * @param {number} position Position to align
 * @param {number} size Alignment size
 * @returns {number} Aligned position
 */
export function alignFilePosition(position, size) {
    const align = size - 1 - (position % size);
    return position + align;
}
/**
 * Read a UINT32 from a byte array (little-endian)
 * @param {Uint8Array} data Data to read a UINT32
 * @param {number} offset data start offset
 * @returns {number} The read UINT32 value
 */
function readUInt32LE(data, offset) {
    return data[offset] | (data[offset + 1] << 8) | (data[offset + 2] << 16) | (data[offset + 3] << 24);
}
// ImageSegment class
export class ImageSegment {
    constructor(addr, data, fileOffs = null, flags = 0) {
        this.addr = addr;
        this.data = data;
        this.fileOffs = fileOffs;
        this.flags = flags;
        this.includeInChecksum = true;
        if (this.addr !== 0) {
            this.padToAlignment(4); // pad all "real" ImageSegments 4 byte aligned length
        }
    }
    copyWithNewAddr(newAddr) {
        return new ImageSegment(newAddr, this.data, 0);
    }
    splitImage(splitLen) {
        const result = new ImageSegment(this.addr, this.data.slice(0, splitLen), 0);
        this.data = this.data.slice(splitLen);
        this.addr += splitLen;
        this.fileOffs = null;
        return result;
    }
    toString() {
        let r = `len 0x${this.data.length.toString(16).padStart(5, "0")} load 0x${this.addr.toString(16).padStart(8, "0")}`;
        if (this.fileOffs !== null) {
            r += ` file_offs 0x${this.fileOffs.toString(16).padStart(8, "0")}`;
        }
        return r;
    }
    getMemoryType(image) {
        return image.ROM_LOADER.MEMORY_MAP.filter((mapRange) => mapRange[0] <= this.addr && this.addr < mapRange[1]).map((mapRange) => mapRange[2]);
    }
    padToAlignment(alignment) {
        this.data = padTo(this.data, alignment, 0);
    }
}
export class ELFSection extends ImageSegment {
    constructor(name, addr, data, flags) {
        super(addr, data, null, flags);
        this.name = name;
    }
    toString() {
        return `${this.name} ${super.toString()}`;
    }
}
export class BaseFirmwareImage {
    constructor(rom) {
        this.SEG_HEADER_LEN = 8;
        this.SHA256_DIGEST_LEN = 32;
        this.ELF_FLAG_WRITE = 0x1;
        this.ELF_FLAG_READ = 0x2;
        this.ELF_FLAG_EXEC = 0x4;
        this.segments = [];
        this.entrypoint = 0;
        this.elfSha256 = null;
        this.elfSha256Offset = 0;
        this.padToSize = 0;
        this.flashMode = 0;
        this.flashSizeFreq = 0;
        this.checksum = 0;
        this.datalength = 0;
        this.IROM_ALIGN = 0;
        this.MMU_PAGE_SIZE_CONF = []; // Default is an empty array
        this.ROM_LOADER = rom;
    }
    loadCommonHeader(data, offset, expectedMagic) {
        const magic = data[offset];
        const segments = data[offset + 1];
        this.flashMode = data[offset + 2];
        this.flashSizeFreq = data[offset + 3];
        this.entrypoint = readUInt32LE(data, offset + 4);
        if (magic !== expectedMagic) {
            throw new ESPError(`Invalid firmware image magic=0x${magic.toString(16)}`);
        }
        return segments;
    }
    verify() {
        if (this.segments.length > 16) {
            throw new ESPError(`Invalid segment count ${this.segments.length} (max 16). ` + "Usually this indicates a linker script problem.");
        }
    }
    loadSegment(data, offset, isIromSegment = false) {
        const fileOffs = offset;
        const segmentAddr = readUInt32LE(data, offset);
        const segmentSize = readUInt32LE(data, offset + 4);
        this.warnIfUnusualSegment(segmentAddr, segmentSize, isIromSegment);
        const segmentData = data.slice(offset + 8, offset + 8 + segmentSize);
        if (segmentData.length < segmentSize) {
            throw new ESPError(`End of file reading segment 0x${segmentAddr.toString(16)}, length ${segmentSize} (actual length ${segmentData.length})`);
        }
        const segment = new ImageSegment(segmentAddr, segmentData, fileOffs);
        this.segments.push(segment);
        return segment;
    }
    warnIfUnusualSegment(offset, size, isIromSegment) {
        if (!isIromSegment) {
            if (offset > 0x40200000 || offset < 0x3ffe0000 || size > 65536) {
                console.warn(`WARNING: Suspicious segment 0x${offset.toString(16)}, length ${size}`);
            }
        }
    }
    maybePatchSegmentData(data, filePos) {
        const segmentLen = data.length;
        if (this.elfSha256Offset >= filePos && this.elfSha256Offset < filePos + segmentLen) {
            const patchOffset = this.elfSha256Offset - filePos;
            if (patchOffset < this.SEG_HEADER_LEN || patchOffset + this.SHA256_DIGEST_LEN > segmentLen) {
                throw new ESPError("Cannot place SHA256 digest on segment boundary" +
                    `(elf_sha256_offset=${this.elfSha256Offset}, file_pos=${filePos}, segment_size=${segmentLen})`);
            }
            const dataPatchOffset = patchOffset - this.SEG_HEADER_LEN;
            const targetArea = data.slice(dataPatchOffset, dataPatchOffset + this.SHA256_DIGEST_LEN);
            const isAllZeros = targetArea.every((byte) => byte === 0);
            if (!isAllZeros) {
                throw new ESPError(`Contents of segment at SHA256 digest offset 0x${this.elfSha256Offset.toString(16)} are not all zero. ` +
                    "Refusing to overwrite.");
            }
            if (!this.elfSha256 || this.elfSha256.length !== this.SHA256_DIGEST_LEN) {
                throw new ESPError("ELF SHA256 digest is not properly initialized");
            }
            const beforePatch = data.slice(0, dataPatchOffset);
            const afterPatch = data.slice(dataPatchOffset + this.SHA256_DIGEST_LEN);
            const newLength = beforePatch.length + this.elfSha256.length + afterPatch.length;
            const result = new Uint8Array(newLength);
            result.set(beforePatch, 0);
            result.set(this.elfSha256, beforePatch.length);
            result.set(afterPatch, beforePatch.length + this.elfSha256.length);
            return result;
        }
        return data;
    }
    saveSegment(output, offset, segment, checksumValue = null) {
        const segmentData = this.maybePatchSegmentData(segment.data, offset);
        // Write segment header
        const view = new DataView(output.buffer, offset);
        view.setUint32(0, segment.addr, true);
        view.setUint32(4, segmentData.length, true);
        // Write segment data
        output.set(segmentData, offset + 8);
        if (checksumValue !== null) {
            return checksum(segmentData, checksumValue);
        }
        return 0;
    }
    saveFlashSegment(output, offset, segment, checksumValue = null) {
        if (this.ROM_LOADER.CHIP_NAME === "ESP32") {
            // Work around a bug in ESP-IDF 2nd stage bootloader, that it didn't map the
            // last MMU page, if an IROM/DROM segment was < 0x24 bytes
            // over the page boundary.
            const segmentEndPos = offset + segment.data.length + this.SEG_HEADER_LEN;
            const segmentLenRemainder = segmentEndPos % this.IROM_ALIGN;
            if (segmentLenRemainder < 0x24) {
                // Create a new array with padding
                const paddedData = new Uint8Array(segment.data.length + (0x24 - segmentLenRemainder));
                paddedData.set(segment.data);
                // Fill the padding with zeros
                paddedData.fill(0, segment.data.length);
                segment.data = paddedData;
            }
        }
        return this.saveSegment(output, offset, segment, checksumValue);
    }
    /**
     * Return ESPLoader checksum from end of just-read image
     * @param {Uint8Array} data image to read checksum from
     * @param {number} offset Current offset in image
     * @returns {number} checksum value
     */
    readChecksum(data, offset) {
        // Skip the padding. The checksum is stored in the last byte so that the
        // file is a multiple of 16 bytes.
        const alignedOffset = alignFilePosition(offset, 16);
        return data[alignedOffset];
    }
    /**
     * Calculate checksum of loaded image, based on segments in segment array.
     * @returns {number} checksum value
     */
    calculateChecksum() {
        let checksumValue = ESP_CHECKSUM_MAGIC;
        for (const seg of this.segments) {
            if (seg.includeInChecksum) {
                checksumValue = checksum(seg.data, checksumValue);
            }
        }
        return checksumValue;
    }
    appendChecksum(output, offset, checksumValue) {
        const alignedOffset = alignFilePosition(offset, 16);
        output[alignedOffset] = checksumValue;
    }
    writeCommonHeader(output, offset, segments) {
        output[offset] = ESP_IMAGE_MAGIC;
        output[offset + 1] = segments;
        output[offset + 2] = this.flashMode;
        output[offset + 3] = this.flashSizeFreq;
        const view = new DataView(output.buffer, offset + 4);
        view.setUint32(0, this.entrypoint, true);
    }
    isIromAddr(addr) {
        return ESP8266ROM.IROM_MAP_START <= addr && addr < ESP8266ROM.IROM_MAP_END;
    }
    getIromSegment() {
        const iromSegments = this.segments.filter((s) => this.isIromAddr(s.addr));
        if (iromSegments.length > 0) {
            if (iromSegments.length !== 1) {
                throw new ESPError(`Found ${iromSegments.length} segments that could be irom0. Bad ELF file?`);
            }
            return iromSegments[0];
        }
        return null;
    }
    getNonIromSegments() {
        const iromSegment = this.getIromSegment();
        return this.segments.filter((s) => s !== iromSegment);
    }
    sortSegments() {
        if (!this.segments.length) {
            return; // nothing to sort
        }
        this.segments.sort((a, b) => a.addr - b.addr);
    }
    mergeAdjacentSegments() {
        if (!this.segments.length) {
            return; // nothing to merge
        }
        const segments = [];
        // The easiest way to merge the sections is to browse them backward.
        for (let i = this.segments.length - 1; i > 0; i--) {
            // elem is the previous section, the one `next_elem` may need to be
            // merged in
            const elem = this.segments[i - 1];
            const nextElem = this.segments[i];
            if (elem.getMemoryType(this).join(",") === nextElem.getMemoryType(this).join(",") &&
                elem.includeInChecksum === nextElem.includeInChecksum &&
                nextElem.addr === elem.addr + elem.data.length &&
                (nextElem.flags & this.ELF_FLAG_EXEC) === (elem.flags & this.ELF_FLAG_EXEC)) {
                // Merge any segment that ends where the next one starts,
                // without spanning memory types
                //
                // (don't 'pad' any gaps here as they may be excluded from the image
                // due to 'noinit' or other reasons.)
                const mergedData = new Uint8Array(elem.data.length + nextElem.data.length);
                mergedData.set(elem.data);
                mergedData.set(nextElem.data, elem.data.length);
                elem.data = mergedData;
            }
            else {
                // The section next_elem cannot be merged into the previous one,
                // which means it needs to be part of the final segments.
                // As we are browsing the list backward, the elements need to be
                // inserted at the beginning of the final list.
                segments.unshift(nextElem);
            }
        }
        // The first segment will always be here as it cannot be merged into any
        // "previous" section.
        segments.unshift(this.segments[0]);
        this.segments = segments;
    }
    setMmuPageSize(size) {
        if (!this.MMU_PAGE_SIZE_CONF && size !== this.IROM_ALIGN) {
            // For chips where MMU page size cannot be set or is fixed, log a warning and use the default.
            console.warn(`WARNING: Changing MMU page size is not supported on ${this.ROM_LOADER.CHIP_NAME}! ` +
                (this.IROM_ALIGN !== 0 ? `Defaulting to ${this.IROM_ALIGN / 1024}KB.` : ""));
        }
        else if (this.MMU_PAGE_SIZE_CONF && !this.MMU_PAGE_SIZE_CONF.includes(size)) {
            // For chips with configurable MMU page sizes, throw an error if the size is invalid.
            const validSizes = this.MMU_PAGE_SIZE_CONF.map((x) => `${x / 1024}KB`).join(", ");
            throw new ESPError(`${size} bytes is not a valid ${this.ROM_LOADER.CHIP_NAME} page size, select from ${validSizes}.`);
        }
        else {
            // Set the MMU page size if valid.
            this.IROM_ALIGN = size;
        }
    }
}
