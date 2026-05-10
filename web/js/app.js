// CYD Digital Frame - Web UI Controller

class FrameController {
    constructor() {
        this.apiBaseUrl = '';
        this.ws = null;
        this.isUploading = false;
        this.uploadedFiles = [];
        this.currentMode = 'sequential';
        this.currentDelay = 3;
        this.toastTimer = null;

        this.init();
    }

    init() {
        console.log('Initializing CYD Digital Frame UI...');

        // Get API base URL
        this.apiBaseUrl = window.location.origin;

        // Bind event listeners
        this.setupEventListeners();

        // Load initial data
        this.updateStatus();
        this.loadFiles();
        this.loadSettings();

        // Start status update interval
        setInterval(() => this.updateStatus(), 5000);
    }

    // ============================================
    // Event Listeners
    // ============================================

    setupEventListeners() {
        // Upload area
        const uploadArea = document.getElementById('upload-area');
        const fileInput = document.getElementById('file-input');

        uploadArea.addEventListener('click', () => fileInput.click());
        uploadArea.addEventListener('dragover', (e) => {
            e.preventDefault();
            uploadArea.classList.add('dragover');
        });
        uploadArea.addEventListener('dragleave', () => {
            uploadArea.classList.remove('dragover');
        });
        uploadArea.addEventListener('drop', (e) => {
            e.preventDefault();
            uploadArea.classList.remove('dragover');
            this.handleFiles(e.dataTransfer.files);
        });

        fileInput.addEventListener('change', (e) => {
            this.handleFiles(e.target.files);
        });

        // Playback controls
        document.getElementById('btn-play').addEventListener('click', (e) => this.withButtonFeedback(e.currentTarget, () => this.sendCommand('play'), 'Play command sent'));
        document.getElementById('btn-pause').addEventListener('click', (e) => this.withButtonFeedback(e.currentTarget, () => this.sendCommand('pause'), 'Pause command sent'));
        document.getElementById('btn-stop').addEventListener('click', (e) => this.withButtonFeedback(e.currentTarget, () => this.sendCommand('stop'), 'Stop command sent'));

        // Playback settings
        document.getElementById('playback-mode').addEventListener('change', (e) => {
            this.currentMode = e.target.value;
            this.sendCommand('set-mode', { mode: this.currentMode });
        });

        document.getElementById('image-delay').addEventListener('input', (e) => {
            this.currentDelay = parseInt(e.target.value);
            document.getElementById('delay-value').textContent = this.currentDelay + 's';
            this.sendCommand('set-delay', { delay: this.currentDelay });
        });

        // File management
        document.getElementById('btn-refresh').addEventListener('click', (e) => this.withButtonFeedback(e.currentTarget, () => this.loadFiles(), 'File list refreshed'));
        document.getElementById('btn-delete-all').addEventListener('click', (e) => this.withButtonFeedback(e.currentTarget, () => this.deleteAllFiles(), 'Deleted all images'));

        // WiFi settings
        document.getElementById('btn-save-wifi').addEventListener('click', (e) => this.withButtonFeedback(e.currentTarget, () => this.saveWiFiSettings(), 'WiFi settings saved'));
    }

    showToast(message, type = 'ok') {
        const toast = document.getElementById('toast');
        if (!toast) return;

        toast.textContent = message;
        toast.className = `toast show ${type}`;
        clearTimeout(this.toastTimer);
        this.toastTimer = setTimeout(() => {
            toast.className = 'toast';
        }, 2400);
    }

    async withButtonFeedback(button, action, successMessage) {
        button.classList.remove('clicked');
        void button.offsetWidth;
        button.classList.add('clicked', 'busy');
        button.disabled = true;

        try {
            const result = await action();
            if (successMessage && result !== false) this.showToast(successMessage);
        } catch (error) {
            this.showToast(error.message || 'Action failed', 'error');
        } finally {
            setTimeout(() => button.classList.remove('clicked'), 180);
            button.classList.remove('busy');
            button.disabled = false;
        }
    }

    getResponseError(responseText, fallback) {
        try {
            const data = JSON.parse(responseText);
            return data.message || fallback;
        } catch (error) {
            return responseText || fallback;
        }
    }

    // ============================================
    // File Upload
    // ============================================

    async handleFiles(files) {
        if (this.isUploading) {
            alert('Upload already in progress');
            return;
        }

        const uploadArea = document.getElementById('upload-area');
        const progressDiv = document.getElementById('upload-progress');

        for (const file of files) {
            if (!this.isValidImageFile(file)) {
                this.showToast(`Invalid file: ${file.name}. Only JPG and GIF allowed.`, 'error');
                continue;
            }

            this.isUploading = true;
            uploadArea.style.display = 'none';
            progressDiv.style.display = 'flex';

            try {
                await this.uploadFile(file);
                this.showToast(`Uploaded ${file.name}`);
                progressDiv.style.display = 'none';
                uploadArea.style.display = 'block';
                this.loadFiles();
            } catch (error) {
                console.error('Upload error:', error);
                this.showToast('Upload failed: ' + error.message, 'error');
                progressDiv.style.display = 'none';
                uploadArea.style.display = 'block';
            } finally {
                this.isUploading = false;
            }
        }
    }

    isValidImageFile(file) {
        const validTypes = ['image/jpeg', 'image/gif'];
        const validExtensions = ['.jpg', '.jpeg', '.gif'];

        const hasValidType = validTypes.includes(file.type);
        const hasValidExtension = validExtensions.some(ext =>
            file.name.toLowerCase().endsWith(ext)
        );

        return hasValidType || hasValidExtension;
    }

    async uploadFile(file) {
        return new Promise((resolve, reject) => {
            const xhr = new XMLHttpRequest();
            const formData = new FormData();
            formData.append('file', file);

            xhr.upload.addEventListener('progress', (e) => {
                if (e.lengthComputable) {
                    const percent = Math.round((e.loaded / e.total) * 100);
                    this.updateProgress(percent, file.name);
                }
            });

            xhr.addEventListener('load', () => {
                if (xhr.status >= 200 && xhr.status < 300) {
                    this.updateProgress(100, file.name);
                    resolve();
                } else {
                    reject(new Error(this.getUploadError(xhr)));
                }
            });

            xhr.addEventListener('error', () => {
                reject(new Error('Network error'));
            });

            xhr.addEventListener('abort', () => {
                reject(new Error('Upload cancelled'));
            });

            xhr.open('POST', `${this.apiBaseUrl}/api/upload`);
            xhr.send(formData);
        });
    }

    getUploadError(xhr) {
        try {
            const response = JSON.parse(xhr.responseText);
            if (response.message) {
                return response.message;
            }
        } catch (error) {
            // Fall through to plain text or status code.
        }

        return xhr.responseText || `HTTP ${xhr.status}`;
    }

    updateProgress(percent, filename) {
        const fill = document.getElementById('progress-fill');
        const text = document.getElementById('progress-text');

        fill.style.width = percent + '%';
        text.textContent = percent + '%';

        console.log(`Upload: ${filename} - ${percent}%`);
    }

    // ============================================
    // File Management
    // ============================================

    async loadFiles() {
        try {
            const response = await fetch(`${this.apiBaseUrl}/api/files`);
            if (!response.ok) throw new Error('Failed to load files');

            const data = await response.json();
            this.displayFiles(data.files || []);

            // Update file count
            document.getElementById('total-images').textContent = data.files?.length || 0;
        } catch (error) {
            console.error('Error loading files:', error);
            document.getElementById('file-list').innerHTML =
                '<div class="no-images">Error loading files</div>';
        }
    }

    displayFiles(files) {
        const fileList = document.getElementById('file-list');

        if (files.length === 0) {
            fileList.innerHTML = '<div class="no-images">No images uploaded yet</div>';
            return;
        }

        fileList.innerHTML = '';

        for (const file of files) {
            const item = document.createElement('div');
            item.className = 'file-item';

            const info = document.createElement('div');
            const name = document.createElement('div');
            name.className = 'file-name';
            name.textContent = file.name;

            const size = document.createElement('div');
            size.className = 'file-size';
            size.textContent = this.formatFileSize(file.size);

            info.appendChild(name);
            info.appendChild(size);

            const actions = document.createElement('div');
            actions.className = 'file-actions';

            const deleteBtn = document.createElement('button');
            deleteBtn.className = 'btn btn-danger btn-small';
            deleteBtn.textContent = 'Delete';
            deleteBtn.addEventListener('click', () => this.withButtonFeedback(deleteBtn, () => this.deleteFile(file.name), `Deleted ${file.name}`));

            actions.appendChild(deleteBtn);

            item.appendChild(info);
            item.appendChild(actions);

            fileList.appendChild(item);
        }
    }

    async deleteFile(filename) {
        if (!confirm(`Delete ${filename}?`)) return false;

        try {
            const response = await fetch(`${this.apiBaseUrl}/api/delete`, {
                method: 'POST',
                headers: { 'Content-Type': 'application/x-www-form-urlencoded' },
                body: `file=${encodeURIComponent(filename)}`
            });

            if (!response.ok) {
                const text = await response.text();
                throw new Error(this.getResponseError(text, 'Delete failed'));
            }
            this.loadFiles();
            return true;
        } catch (error) {
            console.error('Error deleting file:', error);
            this.showToast('Failed to delete file: ' + error.message, 'error');
            throw error;
        }
    }

    async deleteAllFiles() {
        if (!confirm('Delete ALL images? This cannot be undone!')) return false;

        try {
            const response = await fetch(`${this.apiBaseUrl}/api/delete-all`, {
                method: 'POST'
            });

            if (!response.ok) {
                const text = await response.text();
                throw new Error(this.getResponseError(text, 'Delete failed'));
            }
            this.loadFiles();
            return true;
        } catch (error) {
            console.error('Error deleting all files:', error);
            this.showToast('Failed to delete all files: ' + error.message, 'error');
            throw error;
        }
    }

    formatFileSize(bytes) {
        if (bytes === 0) return '0 B';
        const k = 1024;
        const sizes = ['B', 'KB', 'MB', 'GB'];
        const i = Math.floor(Math.log(bytes) / Math.log(k));
        return Math.round(bytes / Math.pow(k, i) * 100) / 100 + ' ' + sizes[i];
    }

    // ============================================
    // Status Updates
    // ============================================

    async updateStatus() {
        try {
            const response = await fetch(`${this.apiBaseUrl}/api/status`);
            if (!response.ok) throw new Error('Status request failed');

            const data = await response.json();

            // Update WiFi status
            const wifiStatus = document.getElementById('wifi-status');
            if (data.wifi_connected) {
                wifiStatus.textContent = '✓ WiFi: ' + data.ip;
                wifiStatus.classList.add('connected');
                wifiStatus.classList.remove('disconnected');
            } else {
                wifiStatus.textContent = '✗ WiFi: Disconnected';
                wifiStatus.classList.remove('connected');
                wifiStatus.classList.add('disconnected');
            }

            // Update SD status
            const sdStatus = document.getElementById('sd-status');
            if (data.sd_ready) {
                sdStatus.textContent = '✓ SD: Ready';
                sdStatus.classList.add('connected');
                sdStatus.classList.remove('disconnected');
            } else {
                sdStatus.textContent = '✗ SD: Not Ready';
                sdStatus.classList.remove('connected');
                sdStatus.classList.add('disconnected');
            }

            // Update device IP
            document.getElementById('device-ip').textContent = data.ip || 'N/A';

        } catch (error) {
            console.error('Error updating status:', error);
        }
    }

    // ============================================
    // Settings
    // ============================================

    async loadSettings() {
        try {
            const response = await fetch(`${this.apiBaseUrl}/api/settings`);
            if (!response.ok) throw new Error('Failed to load settings');

            const data = await response.json();

            if (data.ssid) {
                document.getElementById('wifi-ssid').value = data.ssid;
            }
        } catch (error) {
            console.error('Error loading settings:', error);
        }
    }

    async saveWiFiSettings() {
        const ssid = document.getElementById('wifi-ssid').value.trim();
        const password = document.getElementById('wifi-password').value.trim();

        if (!ssid) {
            this.showToast('SSID is required', 'error');
            return false;
        }

        if (!password) {
            this.showToast('Password is required', 'error');
            return false;
        }

        try {
            const response = await fetch(`${this.apiBaseUrl}/api/settings`, {
                method: 'POST',
                headers: { 'Content-Type': 'application/x-www-form-urlencoded' },
                body: `ssid=${encodeURIComponent(ssid)}&password=${encodeURIComponent(password)}`
            });

            if (!response.ok) {
                const text = await response.text();
                throw new Error(this.getResponseError(text, 'Failed to save settings'));
            }

            this.showToast('WiFi settings saved. Device will reconnect shortly.');
            return true;
        } catch (error) {
            console.error('Error saving settings:', error);
            this.showToast('Failed to save settings: ' + error.message, 'error');
            throw error;
        }
    }

    // ============================================
    // Commands
    // ============================================

    async sendCommand(command, params = {}) {
        console.log('Sending command:', command, params);

        // Commands would be sent to a WebSocket or REST endpoint
        // For now, just log them
    }

    // ============================================
    // WebSocket
    // ============================================

    connectWebSocket() {
        const protocol = window.location.protocol === 'https:' ? 'wss:' : 'ws:';
        const wsUrl = `${protocol}//${window.location.host}/ws`;

        try {
            this.ws = new WebSocket(wsUrl);

            this.ws.addEventListener('open', () => {
                console.log('WebSocket connected');
            });

            this.ws.addEventListener('message', (event) => {
                this.handleWebSocketMessage(JSON.parse(event.data));
            });

            this.ws.addEventListener('close', () => {
                console.log('WebSocket disconnected');
                // Reconnect after 5 seconds
                setTimeout(() => this.connectWebSocket(), 5000);
            });

        } catch (error) {
            console.error('WebSocket error:', error);
        }
    }

    handleWebSocketMessage(data) {
        console.log('WebSocket message:', data);

        if (data.type === 'upload-progress') {
            this.updateProgress(data.percent, data.filename);
        } else if (data.type === 'file-list-updated') {
            this.loadFiles();
        } else if (data.type === 'status-update') {
            this.updateStatus();
        }
    }
}

// Initialize when DOM is ready
document.addEventListener('DOMContentLoaded', () => {
    window.frameController = new FrameController();
});
