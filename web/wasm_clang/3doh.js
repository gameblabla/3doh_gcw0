(() => {
  'use strict';

  const canvas = document.getElementById('screen');
  const ctx = canvas.getContext('2d', { alpha: false });
  const rawCanvas = document.createElement('canvas');
  const rawCtx = rawCanvas.getContext('2d', { alpha: false });
  const status = document.getElementById('status');
  const loader = document.getElementById('loader');
  const biosDrop = document.getElementById('bios-drop');
  const isoDrop = document.getElementById('iso-drop');
  const biosNameLabel = document.getElementById('bios-name');
  const isoNameLabel = document.getElementById('iso-name');
  const fileState = document.getElementById('file-state');
  const biosInput = document.getElementById('bios-input');
  const isoInput = document.getElementById('iso-input');
  const biosChoose = document.getElementById('bios-choose');
  const isoChoose = document.getElementById('iso-choose');
  const helpButton = document.getElementById('help-button');
  const helpDialog = document.getElementById('help-dialog');
  const closeHelp = document.getElementById('close-help');
  const startButton = document.getElementById('start-button');
  const videoStandardSelect = document.getElementById('video-standard');
  const fullscreenButton = document.getElementById('fullscreen-button');
  const pauseButton = document.getElementById('pause-button');
  const resetButton = document.getElementById('reset-button');
  const autocropButton = document.getElementById('autocrop-button');
  const loadGameButton = document.getElementById('load-game-button');
  const audioPrompt = document.getElementById('audio-prompt');
  const bindingsTableBody = document.querySelector('#bindings-table tbody');
  const resetBindingsButton = document.getElementById('reset-bindings');
  const captureBox = document.getElementById('capture-box');
  const captureStatus = document.getElementById('capture-status');
  const cancelCapture = document.getElementById('cancel-capture');

  let wasm = null;
  let memory = null;
  let imageData = null;
  let running = false;
  let biosBytes = null;
  let biosName = '';
  let isoBytes = null;
  let isoName = '';
  let isoSectorSize = 2048;
  let isoSectorOffset = 0;
  let captureAction = null;
  let animationHandle = 0;
  let targetFrameHz = 60;
  let nextFrameTime = 0;
  let statusTimer = 0;
  let autoCrop = true;
  let cropState = { active: null, pending: null, pendingFrames: 0 };
  let displayCrop = null;

  const STORAGE_KEY = '3doh:wasm-clang:bindings:v1';

  const BUTTON = Object.freeze({
    UP: 0, DOWN: 1, LEFT: 2, RIGHT: 3,
    A: 4, B: 5, C: 6, X: 7, L: 8, R: 9, P: 10, EXIT: 11
  });

  const ACTIONS = Object.freeze([
    { id: 'UP', label: 'Up', button: BUTTON.UP },
    { id: 'DOWN', label: 'Down', button: BUTTON.DOWN },
    { id: 'LEFT', label: 'Left', button: BUTTON.LEFT },
    { id: 'RIGHT', label: 'Right', button: BUTTON.RIGHT },
    { id: 'A', label: 'A / Confirm', button: BUTTON.A },
    { id: 'B', label: 'B', button: BUTTON.B },
    { id: 'C', label: 'C', button: BUTTON.C },
    { id: 'X', label: 'X', button: BUTTON.X },
    { id: 'L', label: 'L', button: BUTTON.L },
    { id: 'R', label: 'R', button: BUTTON.R },
    { id: 'P', label: 'Pause / Start', button: BUTTON.P }
  ]);

  const DEFAULT_BINDINGS = Object.freeze({
    keyboard: Object.freeze({
      UP: Object.freeze(['keyboard:key:ArrowUp', 'keyboard:key:w', 'keyboard:key:W']),
      DOWN: Object.freeze(['keyboard:key:ArrowDown', 'keyboard:key:s', 'keyboard:key:S']),
      LEFT: Object.freeze(['keyboard:key:ArrowLeft', 'keyboard:key:a', 'keyboard:key:A']),
      RIGHT: Object.freeze(['keyboard:key:ArrowRight', 'keyboard:key:d', 'keyboard:key:D']),
      A: Object.freeze(['keyboard:key:z', 'keyboard:key:Z', 'keyboard:key:Enter']),
      B: Object.freeze(['keyboard:key:c', 'keyboard:key:C']),
      C: Object.freeze(['keyboard:key:v', 'keyboard:key:V']),
      X: Object.freeze(['keyboard:key:x', 'keyboard:key:X', 'keyboard:key: ']),
      L: Object.freeze(['keyboard:key:q', 'keyboard:key:Q']),
      R: Object.freeze(['keyboard:key:e', 'keyboard:key:E']),
      P: Object.freeze(['keyboard:key:p', 'keyboard:key:P'])
    }),
    gamepad: Object.freeze({
      UP: Object.freeze(['gamepad:button:12', 'gamepad:axis:1:-']),
      DOWN: Object.freeze(['gamepad:button:13', 'gamepad:axis:1:+']),
      LEFT: Object.freeze(['gamepad:button:14', 'gamepad:axis:0:-']),
      RIGHT: Object.freeze(['gamepad:button:15', 'gamepad:axis:0:+']),
      A: Object.freeze(['gamepad:button:0']),
      B: Object.freeze(['gamepad:button:1']),
      C: Object.freeze(['gamepad:button:2']),
      X: Object.freeze(['gamepad:button:3']),
      L: Object.freeze(['gamepad:button:4', 'gamepad:button:6']),
      R: Object.freeze(['gamepad:button:5', 'gamepad:button:7']),
      P: Object.freeze(['gamepad:button:8', 'gamepad:button:9'])
    })
  });

  let bindings = loadBindings();
  const buttonHolders = Array.from({ length: 12 }, () => new Set());

  let audioCtx = null;
  let audioNode = null;
  const audioQueueL = [];
  const audioQueueR = [];

  function clearAudioQueues() {
    audioQueueL.length = 0;
    audioQueueR.length = 0;
  }

  function setStatus(text) {
    const value = text || '';
    status.textContent = value;
    if (statusTimer) {
      clearTimeout(statusTimer);
      statusTimer = 0;
    }
    if (value) {
      statusTimer = setTimeout(() => {
        if (status.textContent === value) status.textContent = '';
        statusTimer = 0;
      }, 1500);
    }
  }

  function fullCropRect(w, h) {
    return { x: 0, y: 0, w: Math.max(1, w | 0), h: Math.max(1, h | 0) };
  }

  function sameCrop(a, b) {
    return !!a && !!b && a.x === b.x && a.y === b.y && a.w === b.w && a.h === b.h;
  }

  function resetCropState() {
    cropState = { active: null, pending: null, pendingFrames: 0 };
    displayCrop = null;
  }

  function detectCropRect(img) {
    const w = img.width | 0;
    const h = img.height | 0;
    const data = img.data;
    let minX = w, minY = h, maxX = -1, maxY = -1;

    for (let y = 0; y < h; y++) {
      let offset = y * w * 4;
      for (let x = 0; x < w; x++, offset += 4) {
        if (data[offset] > 12 || data[offset + 1] > 12 || data[offset + 2] > 12) {
          if (x < minX) minX = x;
          if (x > maxX) maxX = x;
          if (y < minY) minY = y;
          if (y > maxY) maxY = y;
        }
      }
    }

    const full = fullCropRect(w, h);
    if (maxX < minX || maxY < minY) return full;

    // Aggressive mode: no guard band.  Solid/near-black borders, including
    // 1- or 2-pixel borders, should be removed completely.

    const cropW = maxX - minX + 1;
    const cropH = maxY - minY + 1;
    if (cropW < Math.floor(w / 4) || cropH < Math.floor(h / 4)) return full;
    return { x: minX, y: minY, w: cropW, h: cropH };
  }

  function stableCropRect(img) {
    if (!autoCrop) return fullCropRect(img.width, img.height);
    const detected = detectCropRect(img);
    if (!cropState.active) {
      cropState.active = detected;
      cropState.pending = detected;
      cropState.pendingFrames = 0;
      return detected;
    }
    if (sameCrop(detected, cropState.active)) {
      cropState.pending = detected;
      cropState.pendingFrames = 0;
      return cropState.active;
    }
    if (!sameCrop(detected, cropState.pending)) {
      cropState.pending = detected;
      cropState.pendingFrames = 1;
    } else {
      cropState.pendingFrames++;
    }
    if (cropState.pendingFrames >= 6) {
      cropState.active = cropState.pending;
      cropState.pendingFrames = 0;
    }
    return cropState.active;
  }

  function faultStatusFromWasm() {
    if (!wasm || !wasm.exports.threedoh_last_fault_type) return 'Emulation stopped.';
    const fault = wasm.exports.threedoh_last_fault_type();
    if (!fault) return 'Emulation stopped.';
    const pc = wasm.exports.threedoh_last_fault_pc ? wasm.exports.threedoh_last_fault_pc() >>> 0 : 0;
    const addr = wasm.exports.threedoh_last_fault_address ? wasm.exports.threedoh_last_fault_address() >>> 0 : 0;
    const dspres = wasm.exports.threedoh_dsp_resource_fault_count ? wasm.exports.threedoh_dsp_resource_fault_count() >>> 0 : 0;
    const dspaddr = wasm.exports.threedoh_dsp_last_resource_fault_address ? wasm.exports.threedoh_dsp_last_resource_fault_address() >>> 0 : 0;
    const dspdetail = wasm.exports.threedoh_dsp_last_resource_fault_detail ? wasm.exports.threedoh_dsp_last_resource_fault_detail() >>> 0 : 0;
    if (fault === 14)
      return `DSPP resource fault at ${dspaddr.toString(16).padStart(8, '0')} detail ${dspdetail.toString(16)}; count ${dspres}.`;
    return `Strict fault ${fault} at PC ${pc.toString(16).padStart(8, '0')} addr ${addr.toString(16).padStart(8, '0')}.`;
  }

  function readExportU32(name) {
    if (!wasm || !wasm.exports || !wasm.exports[name]) return 0;
    return wasm.exports[name]() >>> 0;
  }

  function readExportU32Arg(name, arg) {
    if (!wasm || !wasm.exports || !wasm.exports[name]) return 0;
    return wasm.exports[name](arg) >>> 0;
  }

  function readDiagnostics() {
    return {
      fault: {
        type: readExportU32('threedoh_last_fault_type'),
        pc: readExportU32('threedoh_last_fault_pc'),
        address: readExportU32('threedoh_last_fault_address'),
        armCurrentPC: readExportU32('threedoh_arm_current_pc'),
        armCurrentCPSR: readExportU32('threedoh_arm_current_cpsr'),
        armFiqEntries: readExportU32('threedoh_arm_fiq_entry_count')
      },
      highRam: {
        reads: readExportU32('threedoh_arm_highram_read_count'),
        writes: readExportU32('threedoh_arm_highram_write_count'),
        firstRead: readExportU32('threedoh_arm_highram_first_read'),
        firstWrite: readExportU32('threedoh_arm_highram_first_write'),
        lastRead: readExportU32('threedoh_arm_highram_last_read'),
        lastWrite: readExportU32('threedoh_arm_highram_last_write')
      },
      dspResource: {
        faults: readExportU32('threedoh_dsp_resource_fault_count'),
        mirrorFaults: readExportU32('threedoh_dsp_resource_mirror_fault_count'),
        lastAddress: readExportU32('threedoh_dsp_last_resource_fault_address'),
        lastDetail: readExportU32('threedoh_dsp_last_resource_fault_detail')
      },
      dspRuntime: {
        runStarts: readExportU32('threedoh_dsp_run_start_count'),
        runStops: readExportU32('threedoh_dsp_run_stop_count'),
        resets: readExportU32('threedoh_dsp_reset_count'),
        intWrites: readExportU32('threedoh_dsp_int_write_count'),
        lastIntValue: readExportU32('threedoh_dsp_last_int_value'),
        pc: readExportU32('threedoh_dsp_current_pc'),
        counter: readExportU32('threedoh_dsp_counter_value'),
        reload: readExportU32('threedoh_dsp_reload_value'),
        status: readExportU32('threedoh_dsp_current_status'),
        audioTicks: readExportU32('threedoh_dsp_audio_tick_count'),
        counterReloads: readExportU32('threedoh_dsp_counter_reload_count'),
        programFrames: readExportU32('threedoh_dsp_program_frame_count'),
        sleeps: readExportU32('threedoh_dsp_sleep_count'),
        deferredTicks: readExportU32('threedoh_dsp_deferred_tick_count'),
        multiReloads: readExportU32('threedoh_dsp_multi_reload_count'),
        audlockWrites: readExportU32('threedoh_dsp_audlock_write_count'),
        audlockResets: readExportU32('threedoh_dsp_audlock_reset_count'),
        lastAudioStatus: readExportU32('threedoh_dsp_last_audio_status_value')
      },
      semaphores: {
        armWrites: readExportU32('threedoh_dsp_arm_sema_write_count'),
        armReads: readExportU32('threedoh_dsp_arm_sema_read_count'),
        dspWrites: readExportU32('threedoh_dsp_dsp_sema_write_count'),
        dspAcks: readExportU32('threedoh_dsp_dsp_sema_ack_count')
      },
      cpuSupply: {
        writes: readExportU32('threedoh_dsp_cpu_supply_write_count'),
        reads: readExportU32('threedoh_dsp_cpu_supply_read_count'),
        randomReads: readExportU32('threedoh_dsp_cpu_supply_random_read_count'),
        lastChannel: readExportU32('threedoh_dsp_last_cpu_supply_channel')
      },
      clioFiq: {
        generated: readExportU32('threedoh_clio_fiq_generate_count'),
        observedPending: readExportU32('threedoh_clio_fiq_need_count'),
        lastReason1: readExportU32('threedoh_clio_last_fiq_reason1'),
        lastReason2: readExportU32('threedoh_clio_last_fiq_reason2'),
        irq0Pending: readExportU32('threedoh_clio_irq0_pending'),
        irq0Mask: readExportU32('threedoh_clio_irq0_mask'),
        irq1Pending: readExportU32('threedoh_clio_irq1_pending'),
        irq1Mask: readExportU32('threedoh_clio_irq1_mask')
      },
      clioFifo: {
        eiReads: readExportU32('threedoh_clio_eififo_read_count'),
        eiEmptyReads: readExportU32('threedoh_clio_eififo_empty_read_count'),
        eiReloads: readExportU32('threedoh_clio_eififo_reload_count'),
        eoWrites: readExportU32('threedoh_clio_eofifo_write_count'),
        eoDisabledWrites: readExportU32('threedoh_clio_eofifo_disabled_write_count'),
        eoFullEvents: readExportU32('threedoh_clio_eofifo_full_count'),
        lastEvent: readExportU32('threedoh_clio_last_fifo_event'),
        eiLastEmptyChannel: readExportU32('threedoh_clio_last_eififo_empty_channel'),
        eiLastReloadChannel: readExportU32('threedoh_clio_last_eififo_reload_channel'),
        eiEmptyCh0: readExportU32Arg('threedoh_clio_eififo_empty_channel_count', 0),
        eiReloadCh0: readExportU32Arg('threedoh_clio_eififo_reload_channel_count', 0),
        eiEmptyCh1: readExportU32Arg('threedoh_clio_eififo_empty_channel_count', 1),
        eiReloadCh1: readExportU32Arg('threedoh_clio_eififo_reload_channel_count', 1),
        eiEmptyCh2: readExportU32Arg('threedoh_clio_eififo_empty_channel_count', 2),
        eiReloadCh2: readExportU32Arg('threedoh_clio_eififo_reload_channel_count', 2),
        eiEmptyCh4: readExportU32Arg('threedoh_clio_eififo_empty_channel_count', 4),
        eiReloadCh4: readExportU32Arg('threedoh_clio_eififo_reload_channel_count', 4),
        eiEmptyCh5: readExportU32Arg('threedoh_clio_eififo_empty_channel_count', 5),
        eiReloadCh5: readExportU32Arg('threedoh_clio_eififo_reload_channel_count', 5),
        levelReasserts: readExportU32('threedoh_clio_fifo_level_reassert_count'),
        levelReassertMask: readExportU32('threedoh_clio_fifo_level_reassert_mask')
      },
      dsppControl: {
        controlWrites: readExportU32('threedoh_clio_dspp_control_write_count'),
        lastControlValue: readExportU32('threedoh_clio_dspp_control_last_value'),
        nonGwWrites: readExportU32('threedoh_clio_dspp_control_non_gw_count'),
        resetWrites: readExportU32('threedoh_clio_dspp_reset_write_count'),
        lastResetValue: readExportU32('threedoh_clio_dspp_reset_last_value'),
        reloadDmaBlockCount: readExportU32('threedoh_clio_fifo_reload_dma_block_count'),
        lastReloadDmaBlockChannel: readExportU32('threedoh_clio_fifo_last_reload_dma_block_channel'),
        nmemReadCount: readExportU32('threedoh_clio_dspp_nmem_read_count'),
        nmemLastReadAddress: readExportU32('threedoh_clio_dspp_nmem_last_read_address'),
        xbusDmaPulseCount: readExportU32('threedoh_clio_xbus_dma_pulse_count'),
        xbusDmaLastLen: readExportU32('threedoh_clio_xbus_dma_last_len'),
        xbusDmaLastAddr: readExportU32('threedoh_clio_xbus_dma_last_addr'),
        xbusDmaTimerAccum: readExportU32('threedoh_clio_xbus_dma_timer_accum'),
        xbusDmaTimerWindow: readExportU32('threedoh_clio_xbus_dma_timer_window'),
        timer120AdjustCount: readExportU32('threedoh_clio_xbus_timer120_adjust_count'),
        timer120LastIn: readExportU32('threedoh_clio_xbus_timer120_last_in'),
        timer120LastOut: readExportU32('threedoh_clio_xbus_timer120_last_out')
      }
    };
  }

  window.threedohDiagnostics = readDiagnostics;

  function setDisplayCrop(crop) {
    if (!crop || crop.w <= 0 || crop.h <= 0) return;
    if (sameCrop(displayCrop, crop) && canvas.width === crop.w && canvas.height === crop.h) return;
    canvas.width = crop.w;
    canvas.height = crop.h;
    document.documentElement.style.setProperty('--aspect', `${crop.w} / ${crop.h}`);
    displayCrop = { x: crop.x, y: crop.y, w: crop.w, h: crop.h };
  }

  function renderFrameImage() {
    if (!imageData) return;
    rawCtx.putImageData(imageData, 0, 0);
    const crop = stableCropRect(imageData);
    setDisplayCrop(crop);
    ctx.clearRect(0, 0, canvas.width, canvas.height);
    ctx.imageSmoothingEnabled = false;
    ctx.drawImage(rawCanvas, crop.x, crop.y, crop.w, crop.h, 0, 0, canvas.width, canvas.height);
  }

  function formatBytes(n) {
    if (!n) return '0 B';
    const units = ['B', 'KiB', 'MiB', 'GiB'];
    let value = n;
    let unit = 0;
    while (value >= 1024 && unit < units.length - 1) { value /= 1024; unit++; }
    return `${value.toFixed(unit ? 1 : 0)} ${units[unit]}`;
  }

  function heapU8() {
    return new Uint8Array(memory.buffer);
  }

  function cstr(ptr) {
    const heap = heapU8();
    let end = ptr;
    while (heap[end] !== 0) end++;
    return new TextDecoder('utf-8').decode(heap.subarray(ptr, end));
  }

  function currentVideoMode() {
    const choice = videoStandardSelect ? videoStandardSelect.value : 'auto';
    const names = `${biosName || ''} ${isoName || ''}`.toLowerCase();
    if (choice === 'pal') return 2;
    if (choice === 'ntsc') return 1;
    if (/\b(pal|euro|europe|eu)\b/.test(names) || names.includes('pal_') || names.includes('pal-')) return 2;
    if (/\b(ntsc|usa|japan|jpn)\b/.test(names) || names.includes('ntsc_') || names.includes('ntsc-')) return 1;
    return 0;
  }

  function resizeCanvasFromWasm() {
    if (!wasm) return;
    const w = wasm.exports.threedoh_width ? wasm.exports.threedoh_width() : 320;
    const h = wasm.exports.threedoh_height ? wasm.exports.threedoh_height() : 240;
    if (rawCanvas.width !== w || rawCanvas.height !== h) {
      rawCanvas.width = w;
      rawCanvas.height = h;
      resetCropState();
      setDisplayCrop(fullCropRect(w, h));
    }
    if (!imageData || imageData.width !== w || imageData.height !== h)
      imageData = rawCtx.createImageData(w, h);
    if (!displayCrop)
      setDisplayCrop(fullCropRect(w, h));
  }

  function cloneDefaultBindings() {
    return {
      keyboard: Object.fromEntries(ACTIONS.map((a) => [a.id, [...(DEFAULT_BINDINGS.keyboard[a.id] || [])]])),
      gamepad: Object.fromEntries(ACTIONS.map((a) => [a.id, [...(DEFAULT_BINDINGS.gamepad[a.id] || [])]]))
    };
  }

  function normalizeBindings(candidate) {
    const out = cloneDefaultBindings();
    if (!candidate || typeof candidate !== 'object') return out;
    for (const type of ['keyboard', 'gamepad']) {
      if (!candidate[type] || typeof candidate[type] !== 'object') continue;
      for (const action of ACTIONS) {
        if (Array.isArray(candidate[type][action.id]))
          out[type][action.id] = candidate[type][action.id].filter((x) => typeof x === 'string');
      }
    }
    return out;
  }

  function loadBindings() {
    try {
      return normalizeBindings(JSON.parse(localStorage.getItem(STORAGE_KEY)));
    } catch (_) {
      return cloneDefaultBindings();
    }
  }

  function saveBindings() {
    try { localStorage.setItem(STORAGE_KEY, JSON.stringify(bindings)); } catch (_) {}
  }

  function tokenLabel(token) {
    if (token.startsWith('keyboard:key:')) {
      const key = token.slice('keyboard:key:'.length);
      if (key === ' ') return 'Space';
      return key;
    }
    if (token.startsWith('gamepad:button:')) return `Button ${token.slice('gamepad:button:'.length)}`;
    if (token.startsWith('gamepad:axis:')) {
      const [, , axis, dir] = token.split(':');
      return `Axis ${axis} ${dir}`;
    }
    return token;
  }

  function renderBindings() {
    bindingsTableBody.textContent = '';
    for (const action of ACTIONS) {
      const tr = document.createElement('tr');
      const name = document.createElement('td');
      name.textContent = action.label;
      const keyboard = document.createElement('td');
      const gamepad = document.createElement('td');
      const controls = document.createElement('td');

      for (const token of bindings.keyboard[action.id] || []) {
        const k = document.createElement('kbd');
        k.textContent = tokenLabel(token);
        keyboard.appendChild(k);
        keyboard.appendChild(document.createTextNode(' '));
      }
      if (!keyboard.childNodes.length) keyboard.textContent = '—';

      for (const token of bindings.gamepad[action.id] || []) {
        const k = document.createElement('kbd');
        k.textContent = tokenLabel(token);
        gamepad.appendChild(k);
        gamepad.appendChild(document.createTextNode(' '));
      }
      if (!gamepad.childNodes.length) gamepad.textContent = '—';

      const bind = document.createElement('button');
      bind.className = 'small-button';
      bind.textContent = 'Bind key';
      bind.addEventListener('click', () => beginCapture(action));
      const clear = document.createElement('button');
      clear.className = 'small-button';
      clear.textContent = 'Clear keys';
      clear.addEventListener('click', () => {
        bindings.keyboard[action.id] = [];
        saveBindings();
        renderBindings();
      });
      controls.append(bind, clear);

      tr.append(name, keyboard, gamepad, controls);
      bindingsTableBody.appendChild(tr);
    }
  }

  function beginCapture(action) {
    captureAction = action;
    captureStatus.textContent = `Press a key for ${action.label}. Escape cancels.`;
    captureBox.hidden = false;
  }

  function stopCapture() {
    captureAction = null;
    captureBox.hidden = true;
  }

  function addKeyboardBinding(action, token) {
    for (const a of ACTIONS)
      bindings.keyboard[a.id] = (bindings.keyboard[a.id] || []).filter((t) => t !== token);
    bindings.keyboard[action.id] = [...(bindings.keyboard[action.id] || []), token];
    saveBindings();
    renderBindings();
  }

  function matchingActionsForKeyboardToken(token) {
    return ACTIONS.filter((a) => (bindings.keyboard[a.id] || []).includes(token));
  }

  function setButtonSource(button, source, pressed) {
    const holders = buttonHolders[button];
    if (!holders || !wasm) return;
    const wasPressed = holders.size > 0;
    if (pressed) holders.add(source);
    else holders.delete(source);
    const isPressed = holders.size > 0;
    if (wasPressed !== isPressed)
      wasm.exports.threedoh_input_button_event(button, isPressed ? 1 : 0);
  }

  function handleKey(event, pressed) {
    const token = `keyboard:key:${event.key}`;
    if (captureAction && pressed) {
      event.preventDefault();
      if (event.key !== 'Escape') addKeyboardBinding(captureAction, token);
      stopCapture();
      return;
    }

    const actions = matchingActionsForKeyboardToken(token);
    if (!actions.length) return;
    event.preventDefault();
    ensureAudio();
    for (const action of actions)
      setButtonSource(action.button, token, pressed);
  }

  function pollGamepads() {
    const pads = navigator.getGamepads ? navigator.getGamepads() : [];
    for (const pad of pads) {
      if (!pad) continue;
      for (const action of ACTIONS) {
        for (const token of bindings.gamepad[action.id] || []) {
          let pressed = false;
          if (token.startsWith('gamepad:button:')) {
            const index = Number(token.slice('gamepad:button:'.length));
            pressed = !!(pad.buttons[index] && pad.buttons[index].pressed);
          } else if (token.startsWith('gamepad:axis:')) {
            const [, , axisText, dir] = token.split(':');
            const axis = Number(axisText);
            const value = Number(pad.axes[axis] || 0);
            pressed = dir === '+' ? value > 0.5 : value < -0.5;
          }
          setButtonSource(action.button, `gamepad:${pad.index}:${token}`, pressed);
          if (pressed) ensureAudio();
        }
      }
    }
  }

  function clearAllInputs() {
    for (let i = 0; i < buttonHolders.length; i++) {
      for (const source of [...buttonHolders[i]]) setButtonSource(i, source, false);
    }
  }

  function setupAudio() {
    if (audioCtx) return;
    const Ctor = window.AudioContext || window.webkitAudioContext;
    if (!Ctor) return;
    audioCtx = new Ctor({ sampleRate: 44100 });
    audioNode = audioCtx.createScriptProcessor(2048, 0, 2);
    audioNode.onaudioprocess = (event) => {
      const left = event.outputBuffer.getChannelData(0);
      const right = event.outputBuffer.getChannelData(1);
      for (let i = 0; i < left.length; i++) {
        left[i] = audioQueueL.length ? audioQueueL.shift() : 0;
        right[i] = audioQueueR.length ? audioQueueR.shift() : 0;
      }
    };
    audioNode.connect(audioCtx.destination);
  }

  function ensureAudio() {
    setupAudio();
    if (!audioCtx) return;
    if (audioCtx.state !== 'running') audioCtx.resume().catch(() => {});
    updateAudioPrompt();
  }

  function updateAudioPrompt() {
    if (!audioPrompt) return;
    const needsGesture = audioCtx && audioCtx.state !== 'running';
    audioPrompt.hidden = !running || !needsGesture;
  }

  function drainAudioFromWasm() {
    if (!wasm) return;
    const count = wasm.exports.threedoh_audio_sample_count();
    if (!count) return;
    const ptr = wasm.exports.threedoh_audio_sample_ptr();
    const view = new DataView(memory.buffer, ptr, count * 4);
    for (let i = 0; i < count; i++) {
      audioQueueL.push(view.getInt16(i * 4, true) / 32768);
      audioQueueR.push(view.getInt16(i * 4 + 2, true) / 32768);
    }
    const maxQueued = 44100 / 2;
    if (audioQueueL.length > maxQueued) {
      audioQueueL.splice(0, audioQueueL.length - maxQueued);
      audioQueueR.splice(0, audioQueueR.length - maxQueued);
    }
  }

  function updateFileState() {
    const bios = biosBytes ? `${biosName} (${formatBytes(biosBytes.length)})` : 'missing';
    const sectorText = isoBytes && isoSectorSize === 2352 ? `, ${isoSectorSize}-byte sectors` : '';
    const iso = isoBytes ? `${isoName} (${formatBytes(isoBytes.length)}${sectorText})` : 'missing';
    if (biosNameLabel) biosNameLabel.textContent = biosBytes ? bios : 'No BIOS selected.';
    if (isoNameLabel) isoNameLabel.textContent = isoBytes ? iso : 'No game image selected.';
    if (biosDrop) biosDrop.classList.toggle('ready', !!biosBytes);
    if (isoDrop) isoDrop.classList.toggle('ready', !!isoBytes);
    fileState.textContent = `BIOS: ${bios}; game image: ${iso}.`;
    startButton.disabled = !(wasm && biosBytes && isoBytes && !running);
    if (resetButton) resetButton.disabled = !(wasm && running);
    if (loadGameButton) loadGameButton.disabled = !(wasm && running);
  }

  async function readFileAsBytes(file) {
    return new Uint8Array(await file.arrayBuffer());
  }

  function basename(name) {
    return String(name || '').replace(/\\/g, '/').split('/').pop().toLowerCase();
  }

  function cueSectorLayout(mode) {
    const upper = String(mode || '').toUpperCase();
    if (upper === 'MODE1/2352') return { sectorSize: 2352, sectorOffset: 16, label: upper };
    if (upper === 'MODE2/2352') return { sectorSize: 2352, sectorOffset: 24, label: upper };
    if (upper === 'MODE1/2048') return { sectorSize: 2048, sectorOffset: 0, label: upper };
    return null;
  }

  function inferSectorLayout(bytes) {
    if (bytes && bytes.length && bytes.length % 2352 === 0)
      return { sectorSize: 2352, sectorOffset: 16, label: 'MODE1/2352 inferred from size' };
    return { sectorSize: 2048, sectorOffset: 0, label: 'MODE1/2048' };
  }

  function parseCueText(text) {
    const lines = String(text || '').split(/\r?\n/);
    let imageFile = '';
    let layout = null;
    for (const line of lines) {
      if (!imageFile) {
        const fileMatch = line.match(/^\s*FILE\s+(?:"([^"]+)"|([^\s].*?))\s+(?:BINARY|MOTOROLA|AIFF|WAVE|MP3)?\s*$/i);
        if (fileMatch) imageFile = (fileMatch[1] || fileMatch[2] || '').trim();
      }
      if (!layout) {
        const trackMatch = line.match(/^\s*TRACK\s+0?1\s+([A-Z0-9]+\/[0-9]+)\s*$/i);
        if (trackMatch) layout = cueSectorLayout(trackMatch[1]);
      }
    }
    return imageFile && layout ? { imageFile, ...layout } : null;
  }

  async function acceptGameFiles(filesLike) {
    const files = Array.from(filesLike || []).filter(Boolean);
    if (!files.length) return;

    const cueFile = files.find((file) => file.name.toLowerCase().endsWith('.cue'));
    if (cueFile) {
      const cueBytes = await readFileAsBytes(cueFile);
      const cueText = new TextDecoder('utf-8').decode(cueBytes);
      const cue = parseCueText(cueText);
      if (!cue) {
        setStatus(`Ignored ${cueFile.name}; unsupported or incomplete CUE file.`);
        return;
      }

      const wanted = basename(cue.imageFile);
      let binFile = files.find((file) => basename(file.name) === wanted);
      if (!binFile) {
        const binCandidates = files.filter((file) => file.name.toLowerCase().endsWith('.bin'));
        if (binCandidates.length === 1) binFile = binCandidates[0];
      }
      if (!binFile) {
        setStatus(`Select or drop ${cueFile.name} together with its referenced BIN file: ${cue.imageFile}.`);
        return;
      }

      const bytes = await readFileAsBytes(binFile);
      if (!bytes.length) {
        setStatus(`Ignored ${binFile.name}; file is empty.`);
        return;
      }
      isoBytes = bytes;
      isoName = `${cueFile.name} -> ${binFile.name} (${cue.label})`;
      isoSectorSize = cue.sectorSize;
      isoSectorOffset = cue.sectorOffset;
      setStatus('BIN/CUE loaded. BIOS is still required before start if not already loaded.');
      updateFileState();
      return;
    }

    const imageFile = files.find((file) => /\.(iso|bin)$/i.test(file.name));
    if (!imageFile) {
      setStatus('Ignored game selection; expected .iso, or a .cue plus its .bin file.');
      return;
    }
    const bytes = await readFileAsBytes(imageFile);
    if (!bytes.length) {
      setStatus(`Ignored ${imageFile.name}; file is empty.`);
      return;
    }
    const layout = inferSectorLayout(bytes);
    isoBytes = bytes;
    isoName = imageFile.name;
    isoSectorSize = layout.sectorSize;
    isoSectorOffset = layout.sectorOffset;
    setStatus(`${imageFile.name} loaded as ${layout.label}. BIOS is still required before start if not already loaded.`);
    updateFileState();
  }

  async function acceptFile(file, forcedType) {
    if (!file) return;
    if (forcedType === 'iso') {
      await acceptGameFiles([file]);
      return;
    }

    const lower = file.name.toLowerCase();
    const type = forcedType || (lower.endsWith('.iso') || lower.endsWith('.cue') ? 'iso' : (lower.includes('bios') || lower.endsWith('.bin') ? 'bios' : 'unknown'));
    if (type === 'unknown') {
      setStatus(`Ignored ${file.name}; expected bios.bin, .iso, or .cue/.bin.`);
      return;
    }
    if (type === 'iso') {
      await acceptGameFiles([file]);
      return;
    }
    if (type === 'bios' && !lower.endsWith('.bin')) {
      setStatus(`Ignored ${file.name}; the BIOS slot expects bios.bin or another .bin BIOS image.`);
      return;
    }
    const bytes = await readFileAsBytes(file);
    if (!bytes.length) {
      setStatus(`Ignored ${file.name}; file is empty.`);
      return;
    }
    biosBytes = bytes;
    biosName = file.name;
    setStatus('BIOS loaded. Game image is still required before start if not already loaded.');
    updateFileState();
  }

  function installDropTarget(element, type) {
    element.addEventListener('click', (event) => {
      if (event.target && (event.target.tagName === 'BUTTON' || event.target.tagName === 'INPUT')) return;
      const input = type === 'bios' ? biosInput : isoInput;
      if (input) input.click();
    });
    element.addEventListener('keydown', (event) => {
      if (event.key !== 'Enter' && event.key !== ' ') return;
      event.preventDefault();
      const input = type === 'bios' ? biosInput : isoInput;
      if (input) input.click();
    });
    for (const eventName of ['dragenter', 'dragover']) {
      element.addEventListener(eventName, (event) => {
        event.preventDefault();
        event.stopPropagation();
        element.classList.add('dragover');
      });
    }
    for (const eventName of ['dragleave', 'drop']) {
      element.addEventListener(eventName, (event) => {
        event.preventDefault();
        event.stopPropagation();
        element.classList.remove('dragover');
      });
    }
    element.addEventListener('drop', (event) => {
      event.preventDefault();
      event.stopPropagation();
      const files = event.dataTransfer && event.dataTransfer.files;
      if (type === 'iso') acceptGameFiles(files).catch((err) => setStatus(String(err && err.message ? err.message : err)));
      else acceptFile(files && files[0], type).catch((err) => setStatus(String(err && err.message ? err.message : err)));
    });
  }

  async function loadWasm() {
    const imports = {
      env: {
        threedoh_host_log(ptr) { console.log(cstr(ptr)); },
        threedoh_host_read_bios(dst, len) {
          if (!biosBytes) return 0;
          const n = Math.min(len >>> 0, biosBytes.length);
          heapU8().set(biosBytes.subarray(0, n), dst >>> 0);
          return n;
        },
        threedoh_host_iso_size() { return isoBytes ? isoBytes.length >>> 0 : 0; },
        threedoh_host_iso_sector_size() { return isoBytes ? isoSectorSize >>> 0 : 0; },
        threedoh_host_iso_sector_offset() { return isoBytes ? isoSectorOffset >>> 0 : 0; },
        threedoh_host_read_iso(offset, dst, len) {
          if (!isoBytes) return 0;
          offset >>>= 0;
          len >>>= 0;
          if (offset >= isoBytes.length) return 0;
          const n = Math.min(len, isoBytes.length - offset);
          heapU8().set(isoBytes.subarray(offset, offset + n), dst >>> 0);
          return n;
        }
      }
    };

    const response = await fetch('3doh.wasm');
    const result = await WebAssembly.instantiateStreaming(response, imports).catch(async () => {
      const bytes = await response.arrayBuffer();
      return WebAssembly.instantiate(bytes, imports);
    });
    wasm = result.instance;
    memory = wasm.exports.memory;
    resizeCanvasFromWasm();
    setStatus('WASM loaded. Drop BIOS and a game image into their separate slots.');
    updateFileState();
  }

  function frameLoop(now) {
    if (!running) return;
    const interval = 1000 / (targetFrameHz || 60);
    if (!nextFrameTime) nextFrameTime = now;
    if (now + 0.25 < nextFrameTime) {
      animationHandle = requestAnimationFrame(frameLoop);
      return;
    }
    while (now >= nextFrameTime + interval) nextFrameTime += interval;
    nextFrameTime += interval;

    pollGamepads();
    const ok = wasm.exports.threedoh_frame();
    if (!ok) {
      running = false;
      setStatus(faultStatusFromWasm());
      updateAudioPrompt();
      return;
    }
    resizeCanvasFromWasm();
    const ptr = wasm.exports.threedoh_framebuffer_ptr();
    imageData.data.set(heapU8().subarray(ptr, ptr + imageData.data.length));
    renderFrameImage();
    drainAudioFromWasm();
    updateAudioPrompt();
    animationHandle = requestAnimationFrame(frameLoop);
  }

  async function startEmulator() {
    if (!biosBytes || !isoBytes) {
      setStatus('Refusing to start: bios.bin and a game image are both required.');
      updateFileState();
      return;
    }
    ensureAudio();
    if (wasm.exports.threedoh_set_video_standard_mode)
      wasm.exports.threedoh_set_video_standard_mode(currentVideoMode());
    const result = wasm.exports.threedoh_start();
    if (result !== 0) {
      setStatus(`3DOh failed to start; error ${result}.`);
      return;
    }
    loader.classList.add('hidden');
    running = true;
    resizeCanvasFromWasm();
    targetFrameHz = wasm.exports.threedoh_frame_rate_hz ? wasm.exports.threedoh_frame_rate_hz() : 60;
    nextFrameTime = 0;
    resetCropState();
    canvas.focus();
    setStatus(`Running at ${targetFrameHz} Hz. Open Controls to remap keys.`);
    updateFileState();
    animationHandle = requestAnimationFrame(frameLoop);
  }

  function softResetEmulator() {
    if (!wasm || !running) return;
    ensureAudio();
    clearAllInputs();
    clearAudioQueues();
    const result = wasm.exports.threedoh_soft_reset();
    if (result !== 0) {
      running = false;
      setStatus(`Soft reset failed; error ${result}.`);
      updateFileState();
      return;
    }
    canvas.focus();
    resizeCanvasFromWasm();
    targetFrameHz = wasm.exports.threedoh_frame_rate_hz ? wasm.exports.threedoh_frame_rate_hz() : targetFrameHz;
    nextFrameTime = 0;
    resetCropState();
    setStatus(`Soft reset complete. Running at ${targetFrameHz} Hz.`);
  }

  function loadNewGame() {
    if (!wasm) return;
    running = false;
    if (animationHandle) cancelAnimationFrame(animationHandle);
    animationHandle = 0;
    nextFrameTime = 0;
    clearAllInputs();
    clearAudioQueues();
    if (wasm.exports.threedoh_is_started && wasm.exports.threedoh_is_started())
      wasm.exports.threedoh_shutdown();

    isoBytes = null;
    isoName = '';
    isoSectorSize = 2048;
    isoSectorOffset = 0;
    resetCropState();
    if (isoInput) isoInput.value = '';
    loader.classList.remove('hidden');
    setStatus(biosBytes ? 'BIOS kept. Drop or choose a new game image, then start.' : 'Drop BIOS and a game image before start.');
    updateFileState();
  }

  function requestFullscreen() {
    const target = document.getElementById('stage');
    if (document.fullscreenElement) document.exitFullscreen().catch(() => {});
    else target.requestFullscreen().catch(() => {});
  }

  document.addEventListener('keydown', (e) => handleKey(e, true), { passive: false });
  document.addEventListener('keyup', (e) => handleKey(e, false), { passive: false });
  window.addEventListener('blur', clearAllInputs);
  window.addEventListener('pointerdown', ensureAudio, { passive: true });
  window.addEventListener('gamepadconnected', () => setStatus('Gamepad connected.'));

  biosInput.addEventListener('change', () => acceptFile(biosInput.files[0], 'bios'));
  isoInput.addEventListener('change', () => acceptGameFiles(isoInput.files));
  biosChoose.addEventListener('click', () => biosInput.click());
  isoChoose.addEventListener('click', () => isoInput.click());
  helpButton.addEventListener('click', () => {
    if (helpDialog && typeof helpDialog.showModal === 'function') helpDialog.showModal();
    else if (helpDialog) helpDialog.setAttribute('open', '');
  });
  closeHelp.addEventListener('click', () => {
    if (helpDialog && typeof helpDialog.close === 'function') helpDialog.close();
    else if (helpDialog) helpDialog.removeAttribute('open');
  });
  startButton.addEventListener('click', startEmulator);
  fullscreenButton.addEventListener('click', requestFullscreen);
  pauseButton.addEventListener('click', () => {
    ensureAudio();
    setButtonSource(BUTTON.P, 'ui:pause', true);
    setTimeout(() => setButtonSource(BUTTON.P, 'ui:pause', false), 80);
    canvas.focus();
  });
  resetButton.addEventListener('click', softResetEmulator);
  if (autocropButton) {
    autocropButton.addEventListener('click', () => {
      autoCrop = !autoCrop;
      autocropButton.textContent = `Auto-crop: ${autoCrop ? 'On' : 'Off'}`;
      resetCropState();
      setStatus(autoCrop ? 'Auto-crop enabled.' : 'Auto-crop disabled.');
      canvas.focus();
    });
  }
  loadGameButton.addEventListener('click', loadNewGame);
  resetBindingsButton.addEventListener('click', () => {
    bindings = cloneDefaultBindings();
    saveBindings();
    renderBindings();
  });
  cancelCapture.addEventListener('click', stopCapture);

  document.addEventListener('dragover', (event) => event.preventDefault());
  document.addEventListener('drop', (event) => {
    event.preventDefault();
    setStatus('Use the separate BIOS and ISO drop boxes.');
  });
  installDropTarget(biosDrop, 'bios');
  installDropTarget(isoDrop, 'iso');

  renderBindings();
  updateFileState();
  loadWasm().catch((err) => setStatus(`WASM load failed: ${err && err.message ? err.message : err}`));
})();
