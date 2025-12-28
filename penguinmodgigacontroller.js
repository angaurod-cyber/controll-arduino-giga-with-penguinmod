
(async function(Scratch) {
    const variables = {};

    if (!Scratch.extensions.unsandboxed) {
        alert("This extension needs to be unsandboxed to run!");
        return;
    }

    // --- Serial helpers (WebSerial) ---
    let port = null;
    let writer = null;
    let reader = null;

    async function connectSerial() {
        if (port) return;
        port = await navigator.serial.requestPort();
        await port.open({ baudRate: 2000000 });
        writer = port.writable.getWriter();
        reader = port.readable.getReader();
        listenSerial();
    }

    async function sendLine(line) {
        if (!writer) return;
        const data = new TextEncoder().encode(line + "\n");
        await writer.write(data);
    }

    // --- Last values cache ---
    let lastTouchX = 0;
    let lastTouchY = 0;
    let lastDigital = 0;
    let lastAnalog = 0;
    let lastDAC0 = 0;
    let lastDAC1 = 0;
    let lastMathResult = 0;

    async function listenSerial() {
        while (true) {
            const { value, done } = await reader.read();
            if (done) break;
            const text = new TextDecoder().decode(value);

            // Robust: handle line-by-line JSON messages
            const lines = text.split(/\r?\n/);
            for (const ln of lines) {
                const t = ln.trim();
                if (!t) continue;
                try {
                    const msg = JSON.parse(t);
                    if (msg.event === "touch") {
                        lastTouchX = Number(msg.x) || 0;
                        lastTouchY = Number(msg.y) || 0;
                    }
                    if (typeof msg.pin !== "undefined") {
                        lastDigital = Number(msg.val) || 0;
                    }
                    if (typeof msg.analog !== "undefined") {
                        lastAnalog = Number(msg.val) || 0;
                    }
                    if (typeof msg.dac0 !== "undefined") {
                        lastDAC0 = Number(msg.dac0) || 0;
                    }
                    if (typeof msg.dac1 !== "undefined") {
                        lastDAC1 = Number(msg.dac1) || 0;
                    }
                    if (typeof msg.result !== "undefined") {
                        lastMathResult = Number(msg.result) || 0;
                    }
                } catch(e) {
                    // ignore non-JSON lines
                }
            }
        }
    }

    // --- ExtForge scaffolding (minimal, correct) ---
    const ExtForge = {
        Broadcasts: new function() {
            this.raw_ = {};
            this.register = (name, blocks) => {
                this.raw_[name] = blocks;
            };
            this.execute = async (name) => {
                if (this.raw_[name]) {
                    await this.raw_[name]();
                }
            };
        },
        Variables: new function() {
            this.raw_ = {};
            this.set = (name, value) => {
                this.raw_[name] = value;
            };
            this.get = (name) => {
                return this.raw_[name] ?? null;
            };
        }
    };

    class Extension {
        getInfo() {
            return {
                id: "arduinoGigaFull",
                name: "Arduino GIGA Full",
                color1: "#0fbd8c",
                blocks: [
                    // Conexión
                    {
                        opcode: "connect",
                        text: "conectar al Arduino GIGA",
                        blockType: "command",
                        arguments: {}
                    },

                    // Pantalla
                    {
                        opcode: "setBackground",
                        text: "fondo [COLOR]",
                        blockType: "command",
                        arguments: { COLOR: { type: "string", defaultValue: "BLACK" } }
                    },
                    {
                        opcode: "clearScreen",
                        text: "limpiar pantalla [COLOR]",
                        blockType: "command",
                        arguments: { COLOR: { type: "string", defaultValue: "WHITE" } }
                    },
                    {
                        opcode: "rotation",
                        text: "rotar pantalla [DEG]",
                        blockType: "command",
                        arguments: { DEG: { type: "number", defaultValue: 0 } }
                    },

                    // Texto avanzado
                    {
                        opcode: "drawText",
                        text: "texto [TEXT] x [X] y [Y] color [COLOR] tamaño [SIZE]",
                        blockType: "command",
                        arguments: {
                            TEXT: { type: "string", defaultValue: "Hola GIGA" },
                            X: { type: "number", defaultValue: 10 },
                            Y: { type: "number", defaultValue: 20 },
                            COLOR: { type: "string", defaultValue: "BLACK" },
                            SIZE: { type: "number", defaultValue: 2 }
                        }
                    },
                    {
                        opcode: "textColor",
                        text: "color de texto [COLOR]",
                        blockType: "command",
                        arguments: { COLOR: { type: "string", defaultValue: "BLACK" } }
                    },
                    {
                        opcode: "textSize",
                        text: "tamaño de texto [SIZE]",
                        blockType: "command",
                        arguments: { SIZE: { type: "number", defaultValue: 2 } }
                    },
                    {
                        opcode: "cursor",
                        text: "cursor x [X] y [Y]",
                        blockType: "command",
                        arguments: { X: { type: "number", defaultValue: 0 }, Y: { type: "number", defaultValue: 0 } }
                    },

                    // Figuras
                    {
                        opcode: "drawLine",
                        text: "línea x1 [X1] y1 [Y1] x2 [X2] y2 [Y2] color [COLOR]",
                        blockType: "command",
                        arguments: {
                            X1: { type: "number", defaultValue: 0 },
                            Y1: { type: "number", defaultValue: 0 },
                            X2: { type: "number", defaultValue: 100 },
                            Y2: { type: "number", defaultValue: 100 },
                            COLOR: { type: "string", defaultValue: "BLACK" }
                        }
                    },
                    {
                        opcode: "drawRect",
                        text: "rectángulo x [X] y [Y] ancho [W] alto [H] color [COLOR]",
                        blockType: "command",
                        arguments: {
                            X: { type: "number", defaultValue: 10 },
                            Y: { type: "number", defaultValue: 10 },
                            W: { type: "number", defaultValue: 60 },
                            H: { type: "number", defaultValue: 40 },
                            COLOR: { type: "string", defaultValue: "BLUE" }
                        }
                    },
                    {
                        opcode: "drawCircle",
                        text: "círculo x [X] y [Y] radio [R] color [COLOR]",
                        blockType: "command",
                        arguments: {
                            X: { type: "number", defaultValue: 50 },
                            Y: { type: "number", defaultValue: 50 },
                            R: { type: "number", defaultValue: 20 },
                            COLOR: { type: "string", defaultValue: "RED" }
                        }
                    },
                    {
                        opcode: "drawTri",
                        text: "triángulo x1 [X1] y1 [Y1] x2 [X2] y2 [Y2] x3 [X3] y3 [Y3] color [COLOR]",
                        blockType: "command",
                        arguments: {
                            X1: { type: "number", defaultValue: 10 },
                            Y1: { type: "number", defaultValue: 10 },
                            X2: { type: "number", defaultValue: 60 },
                            Y2: { type: "number", defaultValue: 10 },
                            X3: { type: "number", defaultValue: 35 },
                            Y3: { type: "number", defaultValue: 50 },
                            COLOR: { type: "string", defaultValue: "GREEN" }
                        }
                    },

                    // Touch
                    { opcode: "touchX", text: "touch X", blockType: "reporter", arguments: {} },
                    { opcode: "touchY", text: "touch Y", blockType: "reporter", arguments: {} },

                    // I/O digital y analógico
                    {
                        opcode: "pinWrite",
                        text: "pin [PIN] salida [VAL]",
                        blockType: "command",
                        arguments: {
                            PIN: { type: "number", defaultValue: 13 },
                            VAL: { type: "string", defaultValue: "HIGH" }
                        }
                    },
                    {
                        opcode: "pinRead",
                        text: "leer pin [PIN]",
                        blockType: "reporter",
                        arguments: { PIN: { type: "number", defaultValue: 13 } }
                    },
                    {
                        opcode: "analogRead",
                        text: "leer analógico [PIN]",
                        blockType: "reporter",
                        arguments: { PIN: { type: "number", defaultValue: 0 } }
                    },

                    // Audio
                    {
                        opcode: "tone",
                        text: "tono pin [PIN] freq [FREQ] dur [DUR]",
                        blockType: "command",
                        arguments: {
                            PIN: { type: "number", defaultValue: 12 },
                            FREQ: { type: "number", defaultValue: 440 },
                            DUR: { type: "number", defaultValue: 200 }
                        }
                    },
                    { opcode: "notone", text: "detener tono pin [PIN]", blockType: "command", arguments: { PIN: { type: "number", defaultValue: 12 } } },

                    // DAC
                    { opcode: "dacWrite", text: "DAC pin [PIN] valor [VAL]", blockType: "command", arguments: { PIN: { type: "number", defaultValue: 66 }, VAL: { type: "number", defaultValue: 2048 } } },
                    { opcode: "dacReporter", text: "dac", blockType: "reporter", arguments: {} },
                    { opcode: "dac1Reporter", text: "dac1", blockType: "reporter", arguments: {} },

                    // 3D
                    { opcode: "cube3d", text: "dibujar cubo 3D", blockType: "command", arguments: {} },
                    { opcode: "rotateCube", text: "rotar cubo [DEG]", blockType: "command", arguments: { DEG: { type: "number", defaultValue: 15 } } },
                    { opcode: "moveCubeX", text: "mover cubo Y [DX]", blockType: "command", arguments: { DX: { type: "number", defaultValue: 10 } } },
                    { opcode: "moveCubeY", text: "mover cubo X [DY]", blockType: "command", arguments: { DY: { type: "number", defaultValue: 10 } } },
                    { opcode: "cubeSize", text: "tamaño cubo [S]", blockType: "command", arguments: { S: { type: "number", defaultValue: 120 } } },

                    // Matemáticas
                    { opcode: "mathEval", text: "math [EXPR]", blockType: "command", arguments: { EXPR: { type: "string", defaultValue: "3+5" } } },
                    { opcode: "mathResult", text: "resultado math", blockType: "reporter", arguments: {} },

                    // Debug
                    { opcode: "ping", text: "ping", blockType: "command", arguments: {} }
                ]
            };
        }

        // --- Impl de bloques ---
        async connect() { await connectSerial(); }

        // Pantalla
        async setBackground(args) { await sendLine(`bg:${args.COLOR}`); }
        async clearScreen(args) { await sendLine(`clear:${args.COLOR}`); }
        async rotation(args) { await sendLine(`rotation:${Number(args.DEG)}`); }

        // Texto
        async drawText(args) {
            const txt = String(args.TEXT).replace(/\n/g, " "); // evitar saltos crudos
            await sendLine(`text:${txt},${Number(args.X)},${Number(args.Y)},${args.COLOR},${Number(args.SIZE)}`);
        }
        async textColor(args) { await sendLine(`textcolor:${args.COLOR}`); }
        async textSize(args) { await sendLine(`textsize:${Number(args.SIZE)}`); }
        async cursor(args) { await sendLine(`cursor:${Number(args.X)},${Number(args.Y)}`); }

        // Figuras
        async drawLine(args) { await sendLine(`line:${Number(args.X1)},${Number(args.Y1)},${Number(args.X2)},${Number(args.Y2)},${args.COLOR}`); }
        async drawRect(args) { await sendLine(`rect:${Number(args.X)},${Number(args.Y)},${Number(args.W)},${Number(args.H)},${args.COLOR}`); }
        async drawCircle(args) { await sendLine(`circle:${Number(args.X)},${Number(args.Y)},${Number(args.R)},${args.COLOR}`); }
        async drawTri(args) {
            await sendLine(`tri:${Number(args.X1)},${Number(args.Y1)},${Number(args.X2)},${Number(args.Y2)},${Number(args.X3)},${Number(args.Y3)},${args.COLOR}`);
        }

        // Touch
        touchX() { return lastTouchX; }
        touchY() { return lastTouchY; }

        // I/O
        async pinWrite(args) { await sendLine(`pin:${Number(args.PIN)},${String(args.VAL).toUpperCase()}`); }
        async pinRead(args) { await sendLine(`pin:${Number(args.PIN)},READ`); return lastDigital; }
        async analogRead(args) { await sendLine(`analog:${Number(args.PIN)}`); return lastAnalog; }

        // Audio
        async tone(args) { await sendLine(`tone:${Number(args.PIN)},${Number(args.FREQ)},${Number(args.DUR)}`); }
        async notone(args) { await sendLine(`notone:${Number(args.PIN)}`); }

        // DAC
        async dacWrite(args) { await sendLine(`dac:${Number(args.PIN)},${Number(args.VAL)}`); }
        dacReporter() { return lastDAC0; }
        dac1Reporter() { return lastDAC1; }

        // 3D
        async cube3d() { await sendLine(`cube3d`); }
        async rotateCube(args) { await sendLine(`rotatecube:${Number(args.DEG)}`); }
        async moveCubeX(args) { await sendLine(`movecubex:${Number(args.DX)}`); }
        async moveCubeY(args) { await sendLine(`movecubey:${Number(args.DY)}`); }
        async cubeSize(args) { await sendLine(`cubesize:${Number(args.S)}`); }

        // Matemáticas
        async mathEval(args) { await sendLine(`math:${String(args.EXPR)}`); }
        mathResult() { return lastMathResult; }

        // Debug
        async ping() { await sendLine(`ping`); }
    }

    let extension = new Extension();
    Scratch.extensions.register(extension);
})(Scratch);