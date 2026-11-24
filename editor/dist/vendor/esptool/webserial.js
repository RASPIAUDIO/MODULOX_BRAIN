/* global SerialPort, ParityType, FlowControlType */
import { ESPError } from "./types/error.js";
import { sleep } from "./util.js";
/**
 * Wrapper class around Webserial API to communicate with the serial device.
 * @param {typeof import("w3c-web-serial").SerialPort} device - Requested device prompted by the browser.
 *
 * ```
 * const port = await navigator.serial.requestPort();
 * ```
 */
class Transport {
    constructor(device, tracing = false, enableSlipReader = true) {
        this.device = device;
        this.tracing = tracing;
        this.slipReaderEnabled = false;
        this.baudrate = 0;
        this.traceLog = "";
        this.lastTraceTime = Date.now();
        this.buffer = new Uint8Array(0);
        this.onDeviceLostCallback = null;
        this.SLIP_END = 0xc0;
        this.SLIP_ESC = 0xdb;
        this.SLIP_ESC_END = 0xdc;
        this.SLIP_ESC_ESC = 0xdd;
        this._DTR_state = false;
        this.slipReaderEnabled = enableSlipReader;
    }
    /**
     * Set callback for when device is lost
     * @param {Function} callback Function to call when device is lost
     */
    setDeviceLostCallback(callback) {
        this.onDeviceLostCallback = callback;
    }
    /**
     * Update the device reference (used when re-selecting device after reset)
     * @param {typeof import("w3c-web-serial").SerialPort} newDevice New SerialPort device
     */
    updateDevice(newDevice) {
        this.device = newDevice;
        this.trace("Device reference updated");
    }
    /**
     * Request the serial device vendor ID and Product ID as string.
     * @returns {string} Return the device VendorID and ProductID from SerialPortInfo as formatted string.
     */
    getInfo() {
        const info = this.device.getInfo();
        return info.usbVendorId && info.usbProductId
            ? `WebSerial VendorID 0x${info.usbVendorId.toString(16)} ProductID 0x${info.usbProductId.toString(16)}`
            : "";
    }
    /**
     * Request the serial device vendor id from SerialPortInfo.
     * @returns {number | undefined} Return the vendor ID.
     */
    getVid() {
        return this.device.getInfo().usbVendorId;
    }
    /**
     * Request the serial device product id from SerialPortInfo.
     * @returns {number | undefined} Return the product ID.
     */
    getPid() {
        return this.device.getInfo().usbProductId;
    }
    /**
     * Format received or sent data for tracing output.
     * @param {string} message Message to format as trace line.
     */
    trace(message) {
        const delta = Date.now() - this.lastTraceTime;
        const prefix = `TRACE ${delta.toFixed(3)}`;
        const traceMessage = `${prefix} ${message}`;
        console.log(traceMessage);
        this.traceLog += traceMessage + "\n";
    }
    async returnTrace() {
        try {
            await navigator.clipboard.writeText(this.traceLog);
            console.log("Text copied to clipboard!");
        }
        catch (err) {
            console.error("Failed to copy text:", err);
        }
    }
    hexify(s) {
        return Array.from(s)
            .map((byte) => byte.toString(16).padStart(2, "0"))
            .join("")
            .padEnd(16, " ");
    }
    hexConvert(uint8Array, autoSplit = true) {
        if (autoSplit && uint8Array.length > 16) {
            let result = "";
            let s = uint8Array;
            while (s.length > 0) {
                const line = s.slice(0, 16);
                const asciiLine = String.fromCharCode(...line)
                    .split("")
                    .map((c) => (c === " " || (c >= " " && c <= "~" && c !== "  ") ? c : "."))
                    .join("");
                s = s.slice(16);
                result += `\n    ${this.hexify(line.slice(0, 8))} ${this.hexify(line.slice(8))} | ${asciiLine}`;
            }
            return result;
        }
        else {
            return this.hexify(uint8Array);
        }
    }
    /**
     * Format data packet using the Serial Line Internet Protocol (SLIP).
     * @param {Uint8Array} data Binary unsigned 8 bit array data to format.
     * @returns {Uint8Array} Formatted unsigned 8 bit data array.
     */
    slipWriter(data) {
        const outData = [];
        outData.push(0xc0);
        for (let i = 0; i < data.length; i++) {
            if (data[i] === 0xdb) {
                outData.push(0xdb, 0xdd);
            }
            else if (data[i] === 0xc0) {
                outData.push(0xdb, 0xdc);
            }
            else {
                outData.push(data[i]);
            }
        }
        outData.push(0xc0);
        return new Uint8Array(outData);
    }
    /**
     * Write binary data to device using the WebSerial device writable stream.
     * @param {Uint8Array} data 8 bit unsigned data array to write to device.
     */
    async write(data) {
        const outData = this.slipWriter(data);
        if (this.device.writable) {
            const writer = this.device.writable.getWriter();
            if (this.tracing) {
                this.trace(`Write ${outData.length} bytes: ${this.hexConvert(outData)}`);
            }
            await writer.write(outData);
            writer.releaseLock();
        }
    }
    /**
     * Append a buffer array after another buffer array
     * @param {Uint8Array} arr1 - First array buffer.
     * @param {Uint8Array} arr2 - magic hex number to select ROM.
     * @returns {Uint8Array} Return a 8 bit unsigned array.
     */
    appendArray(arr1, arr2) {
        const combined = new Uint8Array(arr1.length + arr2.length);
        combined.set(arr1);
        combined.set(arr2, arr1.length);
        return combined;
    }
    /**
     * Read from serial device and append to buffer
     */
    async readLoop() {
        var _a;
        while (this.device.readable) {
            this.reader = (_a = this.device.readable) === null || _a === void 0 ? void 0 : _a.getReader();
            try {
                const { value, done } = await this.reader.read();
                if (done) {
                    this.trace(`Serial port done`);
                    break;
                }
                // The following test is purely precautionary because .read()
                // is not supposed to return empty data when done is false
                if (value && value.length) {
                    const newValue = Uint8Array.from(value);
                    this.buffer = this.appendArray(this.buffer, newValue);
                }
            }
            catch (error) {
                if (error instanceof Error) {
                    // Read retry is possible for the following errors
                    const nonFatal = ["BufferOverrunError", "FramingError", "BreakError", "ParityError"];
                    if (nonFatal.includes(error.name)) {
                        this.trace(`Recoverable serial port error: ${error.message}`);
                        continue;
                    }
                    // Otherwise the read loop cannot continue
                    this.trace(`Unrecoverable serial port error: ${error.message}`);
                    break;
                }
                if (error instanceof DOMException) {
                    // The read loop cannot continue after a DOMException error
                    if (this.onDeviceLostCallback) {
                        this.onDeviceLostCallback();
                    }
                    else {
                        this.trace(`Unrecoverable serial port error: ${error.message}`);
                    }
                    break;
                }
                // The read loop cannot continue after an error whose class is unknown
                this.trace(`Unrecoverable serial port error: ${error}`);
                break;
            }
            finally {
                this.reader.releaseLock();
            }
        }
        // Fatal error or serial port stream ended
        this.trace(`readLoop exited`);
    }
    flushInput() {
        this.buffer = new Uint8Array(0);
    }
    /**
     * Actively drain the input buffer: wait for in-flight bytes to arrive
     * (the ROM repeats an unsupported command response 8 times) and discard them.
     * Flushing alone only drops the bytes already buffered.
     * @param {number} quietMs Time without new bytes before considering the stream drained
     * @param {number} maxMs Upper bound on the total wait
     */
    async drainInput(quietMs = 100, maxMs = 400) {
        const deadline = Date.now() + maxMs;
        let lastLength = -1;
        while (Date.now() < deadline && this.buffer.length !== lastLength) {
            lastLength = this.buffer.length;
            await sleep(quietMs);
        }
        if (this.tracing) {
            this.trace(`Drained ${this.buffer.length} bytes from serial buffer`);
        }
        this.flushInput();
    }
    async flushOutput() {
        try {
            if (this.device.writable) {
                const writer = this.device.writable.getWriter();
                await writer.close();
                writer.releaseLock();
            }
        }
        catch (error) {
            this.trace(`Error while flushing output: ${error}`);
        }
    }
    // `inWaiting` returns the count of bytes in the buffer
    inWaiting() {
        return this.buffer.length;
    }
    // peek at the buffer without removing the data from the buffer
    peek() {
        return this.buffer;
    }
    /**
     * Detect if the data read from device is a Fatal or Guru meditation error.
     * @param {Uint8Array} input Data read from device
     */
    detectPanicHandler(input) {
        const guruMeditationRegex = /G?uru Meditation Error: (?:Core \d panic'ed \(([a-zA-Z ]*)\))?/;
        const fatalExceptionRegex = /F?atal exception \(\d+\): (?:([a-zA-Z ]*)?.*epc)?/;
        const inputString = new TextDecoder("utf-8").decode(input);
        const match = inputString.match(guruMeditationRegex) || inputString.match(fatalExceptionRegex);
        if (match) {
            const cause = match[1] || match[2];
            const msg = `Guru Meditation Error detected${cause ? ` (${cause})` : ""}`;
            throw new Error(msg);
        }
    }
    /**
     * Take a data array and return the first well formed packet after
     * replacing the escape sequence. Reads at least 8 bytes.
     * @param {number} timeout Timeout read data.
     * @returns {Uint8Array} Formatted packet using SLIP escape sequences.
     */
    async read(timeout) {
        let partialPacket = null;
        let isEscaping = false;
        let readBytes = null;
        // eslint-disable-next-line no-constant-condition
        while (true) {
            const timeStamp = Date.now();
            readBytes = new Uint8Array(0);
            // Wait for data to be available, but read all available bytes at once
            while (Date.now() - timeStamp < timeout) {
                if (this.buffer.length > 0) {
                    // Read all available bytes at once instead of one at a time
                    readBytes = this.buffer;
                    this.buffer = new Uint8Array(0);
                    break;
                }
                else {
                    await sleep(1);
                }
            }
            if (!readBytes || readBytes.length === 0) {
                const msg = partialPacket === null
                    ? "Serial data stream stopped: Possible serial noise or corruption."
                    : "No serial data received.";
                if (this.tracing) {
                    this.trace(msg);
                }
                throw new ESPError(msg);
            }
            if (this.tracing) {
                this.trace(`Read ${readBytes.length} bytes: ${this.hexConvert(readBytes)}`);
            }
            for (let i = 0; i < readBytes.length; i++) {
                const byte = readBytes[i];
                if (partialPacket === null) {
                    if (byte === this.SLIP_END) {
                        partialPacket = new Uint8Array(0); // Start of a new packet
                    }
                    else {
                        if (this.tracing) {
                            this.trace(`Read invalid data: ${this.hexConvert(readBytes)}`);
                        }
                        const remainingData = this.buffer;
                        if (this.tracing) {
                            this.trace(`Remaining data in serial buffer: ${this.hexConvert(remainingData)}`);
                        }
                        this.detectPanicHandler(new Uint8Array([...readBytes, ...(remainingData || [])]));
                        throw new ESPError(`Invalid head of packet (0x${byte.toString(16)}): Possible serial noise or corruption.`);
                    }
                }
                else if (isEscaping) {
                    isEscaping = false;
                    if (byte === this.SLIP_ESC_END) {
                        partialPacket = this.appendArray(partialPacket, new Uint8Array([this.SLIP_END]));
                    }
                    else if (byte === this.SLIP_ESC_ESC) {
                        partialPacket = this.appendArray(partialPacket, new Uint8Array([this.SLIP_ESC]));
                    }
                    else {
                        if (this.tracing) {
                            this.trace(`Read invalid data: ${this.hexConvert(readBytes)}`);
                        }
                        const remainingData = this.buffer;
                        if (this.tracing) {
                            this.trace(`Remaining data in serial buffer: ${this.hexConvert(remainingData)}`);
                        }
                        this.detectPanicHandler(new Uint8Array([...readBytes, ...(remainingData || [])]));
                        throw new ESPError(`Invalid SLIP escape (0xdb, 0x${byte.toString(16)})`);
                    }
                }
                else if (byte === this.SLIP_ESC) {
                    isEscaping = true;
                }
                else if (byte === this.SLIP_END) {
                    if (this.tracing) {
                        this.trace(`Received full packet: ${this.hexConvert(partialPacket)}`);
                    }
                    // Put any remaining bytes after SLIP_END back into the buffer
                    if (i + 1 < readBytes.length) {
                        const remainingBytes = readBytes.slice(i + 1);
                        this.buffer = this.appendArray(remainingBytes, this.buffer);
                    }
                    return partialPacket;
                }
                else {
                    partialPacket = this.appendArray(partialPacket, new Uint8Array([byte]));
                }
            }
        }
    }
    /**
     * Read from serial device without SLIP formatting. Calls onData for each chunk.
     * Stops when isClosed() returns true, the stream ends/errors, or disconnect() is called.
     *
     * The reader is stored on the instance so that disconnect() can cancel a pending
     * read: isClosed() is only evaluated after each chunk arrives, so on an idle
     * device a function-local reader would keep the stream locked forever and make
     * disconnect() block in waitForUnlock().
     * @param {Function} onData Callback for each chunk of data read
     * @param {Function} isClosed Function that returns true when reading should stop (e.g. when console is closed)
     */
    async rawRead(onData, isClosed) {
        let reader;
        try {
            if (!this.device.readable) {
                return;
            }
            reader = this.device.readable.getReader();
            this.reader = reader;
            while (!isClosed()) {
                const { value, done } = await reader.read();
                if (done || !value)
                    break;
                if (this.tracing) {
                    this.trace(`Read ${value.length} bytes: ${this.hexConvert(value)}`);
                }
                onData(value);
            }
        }
        catch (error) {
            this.trace(`Error reading from serial port: ${error}`);
            if (error instanceof Error && error.name === "NetworkError" && error.message.includes("device has been lost")) {
                this.trace("Device lost detected (NetworkError)");
                if (this.onDeviceLostCallback) {
                    this.onDeviceLostCallback();
                }
            }
        }
        finally {
            reader === null || reader === void 0 ? void 0 : reader.releaseLock();
            if (this.reader === reader) {
                this.reader = undefined;
            }
        }
    }
    /**
     * Send the RequestToSend (RTS) signal to given state
     * # True for EN=LOW, chip in reset and False EN=HIGH, chip out of reset
     * @param {boolean} state Boolean state to set the signal
     */
    async setRTS(state) {
        await this.device.setSignals({ requestToSend: state });
        // # Work-around for adapters on Windows using the usbser.sys driver:
        // # generate a dummy change to DTR so that the set-control-line-state
        // # request is sent with the updated RTS state and the same DTR state
        // Referenced to esptool.py
        await this.setDTR(this._DTR_state);
    }
    /**
     * Send the dataTerminalReady (DTS) signal to given state
     * # True for IO0=LOW, chip in reset and False IO0=HIGH
     * @param {boolean} state Boolean state to set the signal
     */
    async setDTR(state) {
        this._DTR_state = state;
        await this.device.setSignals({ dataTerminalReady: state });
    }
    /**
     * Set the device signals to given states
     * # True for IO0=LOW, chip in reset and False IO0=HIGH
     * @param {boolean} dtr dataTerminalReady (DTS) signal boolean state
     * @param {boolean} rts requestToSend (RTS) signal boolean state
     * @param {boolean} breakSignal break signal boolean state
     */
    async setSignals(dtr, rts, breakSignal) {
        if (dtr) {
            this._DTR_state = dtr;
        }
        await this.device.setSignals({ dataTerminalReady: dtr, requestToSend: rts, break: breakSignal });
    }
    /**
     * Connect to serial device using the Webserial open method.
     * @param {number} baud Number baud rate for serial connection. Default is 115200.
     * @param {typeof import("w3c-web-serial").SerialOptions} serialOptions Serial Options for WebUSB SerialPort class.
     */
    async connect(baud = 115200, serialOptions = {}) {
        await this.device.open({
            baudRate: baud,
            dataBits: serialOptions === null || serialOptions === void 0 ? void 0 : serialOptions.dataBits,
            stopBits: serialOptions === null || serialOptions === void 0 ? void 0 : serialOptions.stopBits,
            bufferSize: serialOptions === null || serialOptions === void 0 ? void 0 : serialOptions.bufferSize,
            parity: serialOptions === null || serialOptions === void 0 ? void 0 : serialOptions.parity,
            flowControl: serialOptions === null || serialOptions === void 0 ? void 0 : serialOptions.flowControl,
        });
        this.baudrate = baud;
    }
    /**
     * Change the host-side baud rate, preferring an in-place capability when the
     * serial-port implementation provides one.
     * @param {number} baud New baud rate.
     * @param {SerialOptions} serialOptions Options to preserve when a reopen is required.
     * @returns {boolean} True when the port had to be closed and reopened.
     */
    async changeBaudrate(baud, serialOptions = {}) {
        const configurableDevice = this.device;
        if (typeof configurableDevice.setBaudRate === "function") {
            if (this.tracing) {
                this.trace(`Changing host baud rate to ${baud} in place`);
            }
            await configurableDevice.setBaudRate(baud);
            this.baudrate = baud;
            return false;
        }
        if (this.tracing) {
            this.trace(`Reopening serial port at ${baud} baud`);
        }
        await this.disconnect();
        await sleep(50);
        await this.connect(baud, serialOptions);
        await sleep(50);
        this.readLoop();
        return true;
    }
    /**
     * Wait for a given timeout ms for serial device unlock.
     * @param {number} timeout Timeout time in milliseconds (ms) to sleep
     */
    async waitForUnlock(timeout) {
        while ((this.device.readable && this.device.readable.locked) ||
            (this.device.writable && this.device.writable.locked)) {
            await sleep(timeout);
        }
    }
    /**
     * Disconnect from serial device by running SerialPort.close() after streams unlock.
     */
    async disconnect() {
        var _a, _b;
        if ((_a = this.device.readable) === null || _a === void 0 ? void 0 : _a.locked) {
            await ((_b = this.reader) === null || _b === void 0 ? void 0 : _b.cancel());
        }
        await this.waitForUnlock(400);
        await this.device.close();
        this.reader = undefined;
    }
}
export { Transport };
