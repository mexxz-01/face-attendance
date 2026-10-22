/* ═══════════════════════════════════════════════════════════════════════════
 *  OPTIMIZED HIGH-SPEED BIOMETRIC RECOGNITION (TINY DETECTOR)
 *  Supports Automatic Admin Facial Detection & Instant Portal Redirect
 * ═══════════════════════════════════════════════════════════════════════════ */

const MODEL_CDN = 'https://cdn.jsdelivr.net/gh/justadudewhohacks/face-api.js@master/weights';

let isReady = false;
let isLoopRunning = false;
let loopTimer = null;
let currentDescriptor = null;
let isSuspended = false;

/* Fast detector options: 224x224 input size runs in ~15ms */
const detectorOptions = new faceapi.TinyFaceDetectorOptions({
    inputSize: 224,
    scoreThreshold: 0.5
});

window.addEventListener('DOMContentLoaded', async () => {
    await initModels();
    await initCamera();
    startRecognition();
});

/* ── 1. Fast Lightweight Model Initialization ────────────────────────────── */

async function initModels() {
    const badge = document.getElementById('systemStatus');
    const label = document.getElementById('statusLabel');

    try {
        await Promise.all([
            faceapi.nets.tinyFaceDetector.loadFromUri(MODEL_CDN),
            faceapi.nets.faceLandmark68Net.loadFromUri(MODEL_CDN),
            faceapi.nets.faceRecognitionNet.loadFromUri(MODEL_CDN)
        ]);
        isReady = true;
        badge.classList.add('ready');
        label.textContent = 'Neural Core Online';
        console.log('[FAST_AI] Tiny models loaded successfully');
    } catch (err) {
        label.textContent = 'Model Loading Failed';
        setBannerState('noface', 'System Offline', 'Unable to fetch neural network weights from CDN.');
        console.error('[AI_ERR]', err);
    }
}

/* ── 2. Hardware Accelerated Camera Feed ─────────────────────────────────── */

async function initCamera() {
    const video = document.getElementById('webcam');
    try {
        const stream = await navigator.mediaDevices.getUserMedia({
            video: {
                width: { ideal: 640 },
                height: { ideal: 480 },
                facingMode: 'user'
            },
            audio: false
        });
        video.srcObject = stream;
        await new Promise(res => { video.onloadedmetadata = res; });
        const canvas = document.getElementById('canvasOverlay');
        canvas.width = video.videoWidth || 640;
        canvas.height = video.videoHeight || 480;
    } catch (err) {
        setBannerState('noface', 'Camera Inaccessible', 'Please allow camera permissions and refresh.');
        console.error('[CAMERA_ERR]', err);
    }
}

/* ── 3. High Performance Recognition Loop ────────────────────────────────── */

function startRecognition() {
    if (!isReady) return;
    isLoopRunning = true;
    setBannerState('standby', 'Scanning Face...', 'Please look directly at the screen');
    processFrame();
}

function stopRecognition() {
    isLoopRunning = false;
    if (loopTimer) {
        clearTimeout(loopTimer);
        loopTimer = null;
    }
}

function pauseCycle(durationMs) {
    isSuspended = true;
    setTimeout(() => {
        isSuspended = false;
        closeRegisterDrawer();
        startRecognition();
    }, durationMs);
}

async function processFrame() {
    if (!isLoopRunning || isSuspended) return;

    const video = document.getElementById('webcam');
    const canvas = document.getElementById('canvasOverlay');
    const ctx = canvas.getContext('2d', { willReadFrequently: true });

    try {
        const detection = await faceapi
            .detectSingleFace(video, detectorOptions)
            .withFaceLandmarks()
            .withFaceDescriptor();

        ctx.clearRect(0, 0, canvas.width, canvas.height);

        if (!detection) {
            setBannerState('noface', "Face didn't detected", 'Position your face clearly within the frame');
            nextFrame(600);
            return;
        }

        drawReticle(ctx, detection.detection.box);

        const descriptor = Array.from(detection.descriptor);
        const res = await fetch('/api/verify', {
            method: 'POST',
            headers: { 'Content-Type': 'application/json' },
            body: JSON.stringify({ descriptor })
        });
        const data = await res.json();

        if (data.matched) {
            stopRecognition();
            drawReticle(ctx, detection.detection.box, true);

            /* ── ADMIN FACIAL RECOGNITION & INSTANT REDIRECT ── */
            if (data.is_admin && data.admin_token) {
                sessionStorage.setItem('imperial_admin_token', data.admin_token);
                setBannerState('confirmed', `Welcome Admin: ${data.name}`, 'Administrator recognized • Redirecting to Admin Portal...');
                setTimeout(() => {
                    window.location.href = '/admin';
                }, 1200);
                return;
            }

            /* Regular Student Confirmation */
            setBannerState('confirmed', `${data.name} + attendance confirmed`, `Status: PRESENT • Recorded on ${data.date}`);
            pauseCycle(3500);
        } else {
            /* New face detected */
            stopRecognition();
            currentDescriptor = descriptor;
            drawReticle(ctx, detection.detection.box, false, true);
            setBannerState('newface', 'New face detected', 'Enter your name to register');
            openRegisterDrawer();
        }

    } catch (err) {
        console.error('[FRAME_ERR]', err);
        nextFrame(800);
    }
}

function nextFrame(delayMs) {
    loopTimer = setTimeout(() => {
        if (isLoopRunning && !isSuspended) {
            processFrame();
        }
    }, delayMs);
}

/* ── 4. Smooth Metallic Reticle ──────────────────────────────────────────── */

function drawReticle(ctx, box, isConfirmed = false, isNewFace = false) {
    const strokeColor = isConfirmed ? '#f7d070' : (isNewFace ? '#e8cb76' : '#d4af37');
    const pad = 12;
    const x = box.x - pad;
    const y = box.y - pad;
    const w = box.width + pad * 2;
    const h = box.height + pad * 2;
    const r = 8;

    ctx.save();
    ctx.lineWidth = 2.5;
    ctx.strokeStyle = strokeColor;
    ctx.shadowColor = isConfirmed ? 'rgba(247, 208, 112, 0.6)' : 'rgba(212, 175, 55, 0.35)';
    ctx.shadowBlur = 15;

    ctx.beginPath();
    ctx.moveTo(x + r, y);
    ctx.lineTo(x + w - r, y); ctx.arcTo(x + w, y, x + w, y + r, r);
    ctx.lineTo(x + w, y + h - r); ctx.arcTo(x + w, y + h, x + w - r, y + h, r);
    ctx.lineTo(x + r, y + h); ctx.arcTo(x, y + h, x, y + h - r, r);
    ctx.lineTo(x, y + r); ctx.arcTo(x, y, x + r, y, r);
    ctx.closePath();
    ctx.stroke();

    ctx.restore();
}

/* ── 5. Clean Registration Drawer ────────────────────────────────────────── */

function openRegisterDrawer() {
    const drawer = document.getElementById('registerDrawer');
    drawer.classList.add('active');
    const input = document.getElementById('studentNameInput');
    input.value = '';
    setTimeout(() => input.focus(), 120);
}

function closeRegisterDrawer() {
    const drawer = document.getElementById('registerDrawer');
    drawer.classList.remove('active');
    currentDescriptor = null;
}

async function submitRegistration(event) {
    if (event) event.preventDefault();

    const input = document.getElementById('studentNameInput');
    const name = input.value.trim();
    if (!name) {
        input.focus();
        return;
    }

    if (!currentDescriptor) {
        setBannerState('noface', 'Session Expired', 'Re-scanning face...');
        closeRegisterDrawer();
        pauseCycle(1500);
        return;
    }

    const btn = document.getElementById('btnRegister');
    btn.disabled = true;
    btn.textContent = 'Enrolling...';

    try {
        const res = await fetch('/api/register', {
            method: 'POST',
            headers: { 'Content-Type': 'application/json' },
            body: JSON.stringify({ name, descriptor: currentDescriptor })
        });
        const data = await res.json();

        if (data.success) {
            closeRegisterDrawer();
            setBannerState('confirmed', `${data.name || name} + attendance confirmed`, `Enrolled & marked present for ${data.date || 'today'}`);
            pauseCycle(3800);
        } else {
            setBannerState('noface', 'Registration Failed', data.error || 'Server error');
            pauseCycle(2500);
        }
    } catch (err) {
        setBannerState('noface', 'Network Fault', err.message);
        pauseCycle(2500);
    }

    btn.disabled = false;
    btn.innerHTML = `
        <span>Register &amp; Mark Present</span>
        <svg width="18" height="18" viewBox="0 0 24 24" fill="none" stroke="currentColor" stroke-width="2">
            <line x1="5" y1="12" x2="19" y2="12"></line>
            <polyline points="12 5 19 12 12 19"></polyline>
        </svg>`;
}

/* ── 6. State Banner Helper ──────────────────────────────────────────────── */

function setBannerState(state, title, desc) {
    const banner = document.getElementById('liveBanner');
    const icon = document.getElementById('bannerIcon');
    const titleEl = document.getElementById('bannerTitle');
    const descEl = document.getElementById('bannerDesc');

    banner.className = 'live-banner banner-' + state;
    titleEl.textContent = title;
    descEl.textContent = desc || '';

    if (state === 'confirmed') {
        icon.innerHTML = `
            <svg width="24" height="24" viewBox="0 0 24 24" fill="none" stroke="currentColor" stroke-width="2.5">
                <polyline points="20 6 9 17 4 12"></polyline>
            </svg>`;
    } else if (state === 'noface') {
        icon.innerHTML = `
            <svg width="24" height="24" viewBox="0 0 24 24" fill="none" stroke="currentColor" stroke-width="2">
                <circle cx="12" cy="12" r="10"></circle>
                <line x1="15" y1="9" x2="9" y2="15"></line>
                <line x1="9" y1="9" x2="15" y2="15"></line>
            </svg>`;
    } else if (state === 'newface') {
        icon.innerHTML = `
            <svg width="24" height="24" viewBox="0 0 24 24" fill="none" stroke="currentColor" stroke-width="2">
                <path d="M16 21v-2a4 4 0 0 0-4-4H5a4 4 0 0 0-4 4v2"></path>
                <circle cx="8.5" cy="7" r="4"></circle>
                <line x1="20" y1="8" x2="20" y2="14"></line>
                <line x1="23" y1="11" x2="17" y2="11"></line>
            </svg>`;
    } else {
        icon.innerHTML = `
            <svg width="24" height="24" viewBox="0 0 24 24" fill="none" stroke="currentColor" stroke-width="2">
                <circle cx="12" cy="12" r="10"></circle>
                <path d="M12 6v6l4 2"></path>
            </svg>`;
    }
}
