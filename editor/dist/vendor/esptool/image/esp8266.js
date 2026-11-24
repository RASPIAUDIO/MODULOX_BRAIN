import { ESP8266ROM } from "../targets/esp8266.js";
import { bstrToUi8 } from "../util.js";
import { BaseFirmwareImage, ESP_IMAGE_MAGIC } from "./base.js";
export class ESP8266ROMFirmwareImage extends BaseFirmwareImage {
    constructor(rom, loadFile = null) {
        super(rom);
        this.version = 1;
        this.ROM_LOADER = rom;
        this.flashMode = 0;
        this.flashSizeFreq = 0;
        if (loadFile !== null) {
            this.loadFromFile(loadFile);
        }
    }
    loadFromFile(file) {
        const binaryData = file instanceof Uint8Array ? file : bstrToUi8(file);
        let offset = 0;
        const segments = this.loadCommonHeader(binaryData, offset, ESP_IMAGE_MAGIC);
        offset += 8;
        for (let i = 0; i < segments; i++) {
            const segment = this.loadSegment(binaryData, offset);
            offset += 8 + segment.data.length;
        }
        this.checksum = this.readChecksum(binaryData, offset);
        this.verify();
    }
    defaultOutputName(inputFile) {
        return inputFile + "-";
    }
}
export class ESP8266V2FirmwareImage extends BaseFirmwareImage {
    constructor(rom, loadFile = null) {
        super(rom);
        this.version = 2;
        this.ROM_LOADER = rom;
        this.flashMode = 0;
        this.flashSizeFreq = 0;
        if (loadFile !== null) {
            this.loadFromFile(loadFile);
        }
    }
    async loadFromFile(fileStr) {
        const binaryData = fileStr instanceof Uint8Array ? fileStr : bstrToUi8(fileStr);
        let offset = 0;
        // Load first header
        const segments = this.loadCommonHeader(binaryData, offset, ESP8266V2FirmwareImage.IMAGE_V2_MAGIC);
        offset += 8;
        if (segments !== ESP8266V2FirmwareImage.IMAGE_V2_SEGMENT) {
            console.warn(`Warning: V2 header has unexpected "segment" count ${segments} (usually 4)`);
        }
        // Save first header values
        const firstFlashMode = this.flashMode;
        const firstFlashSizeFreq = this.flashSizeFreq;
        const firstEntrypoint = this.entrypoint;
        // irom segment comes before the second header
        const iromSegment = this.loadSegment(binaryData, offset, true);
        // for actual mapped addr, add ESP8266ROM.IROM_MAP_START + flashing_addr + 8
        iromSegment.addr = 0;
        iromSegment.includeInChecksum = false;
        offset += 8 + iromSegment.data.length;
        // Load the second header
        const secondSegments = this.loadCommonHeader(binaryData, offset, ESP_IMAGE_MAGIC);
        offset += 8;
        // Compare headers
        if (firstFlashMode !== this.flashMode) {
            console.warn(`WARNING: Flash mode value in first header (0x${firstFlashMode.toString(16)}) disagrees with second (0x${this.flashMode.toString(16)}). Using second value.`);
        }
        if (firstFlashSizeFreq !== this.flashSizeFreq) {
            console.warn(`WARNING: Flash size/freq value in first header (0x${firstFlashSizeFreq.toString(16)}) disagrees with second (0x${this.flashSizeFreq.toString(16)}). Using second value.`);
        }
        if (firstEntrypoint !== this.entrypoint) {
            console.warn(`WARNING: Entrypoint address in first header (0x${firstEntrypoint.toString(16)}) disagrees with second header (0x${this.entrypoint.toString(16)}). Using second value.`);
        }
        // Load all the usual segments
        for (let i = 0; i < secondSegments; i++) {
            const segment = this.loadSegment(binaryData, offset);
            offset += 8 + segment.data.length;
        }
        this.checksum = this.readChecksum(binaryData, offset);
        this.verify();
    }
    defaultOutputName(inputFile) {
        const iromSegment = this.getIromSegment();
        let iromOffs = 0;
        if (iromSegment !== null) {
            iromOffs = iromSegment.addr - ESP8266ROM.IROM_MAP_START;
        }
        // Get the base name without extension
        const baseName = inputFile.replace(/\.[^/.]+$/, "");
        const FLASH_SECTOR_SIZE = 0x1000;
        // Calculate the offset aligned to flash sector size
        const alignedOffset = iromOffs & ~(FLASH_SECTOR_SIZE - 1);
        return `${baseName}-0x${alignedOffset.toString(16).padStart(5, "0")}.bin`;
    }
}
// First byte of the "v2" application image
ESP8266V2FirmwareImage.IMAGE_V2_MAGIC = 0xea;
// First 'segment' value in a "v2" application image
ESP8266V2FirmwareImage.IMAGE_V2_SEGMENT = 4;
