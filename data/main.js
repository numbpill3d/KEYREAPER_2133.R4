// PN-2133 Dashboard Controller
// field-tested interface for covert rfid operations
// built for reliability under stress, not pretty demos

class PN2133Interface {
    constructor() {
        this.activeTab = 'scanner';
        this.systemOnline = false;
        this.entropyInterval = null;
        this.logsInterval = null;
        this.settingsCache = {};
        
        this.init();
    }
    
    init() {
        this.setupEventListeners();
        this.initializeTabs();
        this.checkSystemStatus();
        this.loadSettings();
        this.startSystemClock();
        
        // start monitoring loops
        this.startEntropyMonitoring();
        this.startLogMonitoring();
        
        console.log('[PN2133] interface initialized. standing by.');
    }
    
    setupEventListeners() {
        // tab navigation
        document.querySelectorAll('.tab-button').forEach(button => {
            button.addEventListener('click', (e) => {
                this.switchTab(e.target.dataset.tab);
            });
        });
        
        // scanner controls
        document.getElementById('scanButton').addEventListener('click', () => {
            this.performManualScan();
        });
        
        document.getElementById('writeButton').addEventListener('click', () => {
            this.writeToTag();
        });
        
        // logger controls
        document.getElementById('silentLogging').addEventListener('change', (e) => {
            this.toggleSilentLogging(e.target.checked);
        });
        
        document.getElementById('scanInterval').addEventListener('change', (e) => {
            this.updateScanInterval(parseInt(e.target.value));
        });
        
        // settings controls
        document.getElementById('saveSettings').addEventListener('click', () => {
            this.saveSettings();
        });
        
        document.getElementById('resetSettings').addEventListener('click', () => {
            this.resetSettings();
        });
        
        // data controls
        document.getElementById('refreshLogs').addEventListener('click', () => {
            this.refreshLogs();
        });
        
        document.getElementById('exportLogs').addEventListener('click', () => {
            this.exportLogs();
        });
        
        document.getElementById('clearLogs').addEventListener('click', () => {
            this.clearLogs();
        });
    }
    
    initializeTabs() {
        this.switchTab('scanner');
    }
    
    switchTab(tabName) {
        // clear active states
        document.querySelectorAll('.tab-button').forEach(btn => {
            btn.classList.remove('active');
        });
        document.querySelectorAll('.tab-panel').forEach(panel => {
            panel.classList.remove('active');
        });
        
        // set new active states
        document.querySelector(`[data-tab="${tabName}"]`).classList.add('active');
        document.getElementById(tabName).classList.add('active');
        
        this.activeTab = tabName;
        
        // tab-specific initialization
        if (tabName === 'data') {
            this.refreshLogs();
        }
    }
    
    async checkSystemStatus() {
        try {
            const response = await fetch('/api/settings');
            if (response.ok) {
                this.systemOnline = true;
                this.updateSystemStatus('ONLINE', 'system operational');
            } else {
                throw new Error('api unreachable');
            }
        } catch (error) {
            this.systemOnline = false;
            this.updateSystemStatus('OFFLINE', 'connection failed');
            console.error('[PN2133] system check failed:', error.message);
        }
    }
    
    updateSystemStatus(status, info) {
        const statusElement = document.getElementById('systemStatus');
        const infoElement = document.getElementById('connectionInfo');
        
        statusElement.textContent = status;
        statusElement.className = `status-indicator ${status.toLowerCase()}`;
        infoElement.textContent = info;
    }
    
    async performManualScan() {
        const button = document.getElementById('scanButton');
        const resultsDiv = document.getElementById('scanResults');
        
        button.disabled = true;
        button.textContent = 'SCANNING...';
        
        resultsDiv.innerHTML = '<div class="placeholder-text">[ SCANNING FOR TAG... ]<br>Keep tag near reader</div>';
        
        try {
            const response = await fetch('/api/scan');
            const data = await response.json();
            
            if (data.success) {
                this.displayScanResults(data);
            } else {
                this.displayScanError(data.error);
            }
        } catch (error) {
            this.displayScanError('network error: ' + error.message);
        } finally {
            button.disabled = false;
            button.textContent = 'SCAN FOR TAG';
        }
    }
    
    displayScanResults(data) {
        const resultsDiv = document.getElementById('scanResults');
        
        let html = `
            <div class="log-entry">
                <div class="log-uid">${data.uid}</div>
                <div class="log-meta">
                    Type: ${data.tag_type} | 
                    Time: ${new Date(parseInt(data.timestamp)).toLocaleTimeString()}
                </div>`;
        
        if (data.ndef_data) {
            html += `<div class="log-data">NDEF: ${data.ndef_data}</div>`;
        }
        
        html += '</div>';
        resultsDiv.innerHTML = html;
    }
    
    displayScanError(error) {
        const resultsDiv = document.getElementById('scanResults');
        resultsDiv.innerHTML = `
            <div class="placeholder-text text-warning">
                [ SCAN FAILED ]<br>${error}
            </div>`;
    }
    
    async writeToTag() {
        const writeData = document.getElementById('writeData').value.trim();
        const button = document.getElementById('writeButton');
        
        if (!writeData) {
            alert('enter data to write first');
            return;
        }
        
        button.disabled = true;
        button.textContent = 'WRITING...';
        
        try {
            const response = await fetch('/api/write', {
                method: 'POST',
                headers: { 'Content-Type': 'application/json' },
                body: JSON.stringify({ data: writeData })
            });
            
            const result = await response.json();
            
            if (result.success) {
                alert('write successful');
                document.getElementById('writeData').value = '';
            } else {
                alert('write failed: ' + result.error);
            }
        } catch (error) {
            alert('write error: ' + error.message);
        } finally {
            button.disabled = false;
            button.textContent = 'WRITE TO TAG';
        }
    }
    
    async toggleSilentLogging(enabled) {
        try {
            const response = await fetch('/api/settings', {
                method: 'POST',
                headers: { 'Content-Type': 'application/json' },
                body: JSON.stringify({ silent_logging: enabled })
            });
            
            if (response.ok) {
                const statusElement = document.getElementById('loggingStatus');
                statusElement.textContent = enabled ? 'ACTIVE' : 'DISABLED';
                statusElement.className = `status-text ${enabled ? 'enabled' : 'disabled'}`;
            }
        } catch (error) {
            console.error('[PN2133] failed to toggle logging:', error);
        }
    }
    
    async updateScanInterval(interval) {
        try {
            await fetch('/api/settings', {
                method: 'POST',
                headers: { 'Content-Type': 'application/json' },
                body: JSON.stringify({ scan_interval: interval })
            });
        } catch (error) {
            console.error('[PN2133] failed to update scan interval:', error);
        }
    }
    
    startEntropyMonitoring() {
        this.entropyInterval = setInterval(async () => {
            if (!this.systemOnline) return;
            
            try {
                const response = await fetch('/api/entropy');
                const data = await response.json();
                
                document.getElementById('entropyCurrent').textContent = data.current;
                document.getElementById('entropyBaseline').textContent = data.baseline;
                document.getElementById('entropyDeviation').textContent = data.deviation;
                
                const statusElement = document.getElementById('entropyStatus');
                if (data.paranormal_active) {
                    statusElement.textContent = 'ANOMALY';
                    statusElement.className = 'entropy-status anomaly';
                } else {
                    statusElement.textContent = 'NORMAL';
                    statusElement.className = 'entropy-status';
                }
            } catch (error) {
                // silent fail - entropy monitoring is background task
            }
        }, 500);
    }
    
    startLogMonitoring() {
        this.logsInterval = setInterval(() => {
            if (this.activeTab === 'data') {
                this.refreshLogs();
            }
        }, 2000);
    }
    
    async loadSettings() {
        try {
            const response = await fetch('/api/settings');
            const settings = await response.json();
            
            // update ui elements
            document.getElementById('silentLogging').checked = settings.silent_logging;
            document.getElementById('scanInterval').value = settings.scan_interval;
            document.getElementById('entropyThreshold').value = settings.entropy_threshold;
            document.getElementById('entropyMonitoring').checked = settings.entropy_monitoring;
            
            // update status displays
            const loggingStatus = document.getElementById('loggingStatus');
            loggingStatus.textContent = settings.silent_logging ? 'ACTIVE' : 'DISABLED';
            loggingStatus.className = `status-text ${settings.silent_logging ? 'enabled' : 'disabled'}`;
            
            this.settingsCache = settings;
        } catch (error) {
            console.error('[PN2133] failed to load settings:', error);
        }
    }
    
    async saveSettings() {
        const settings = {
            silent_logging: document.getElementById('silentLogging').checked,
            entropy_monitoring: document.getElementById('entropyMonitoring').checked,
            scan_interval: parseInt(document.getElementById('scanInterval').value),
            entropy_threshold: parseInt(document.getElementById('entropyThreshold').value),
            supabase_url: document.getElementById('supabaseUrl').value,
            supabase_key: document.getElementById('supabaseKey').value
        };
        
        try {
            const response = await fetch('/api/settings', {
                method: 'POST',
                headers: { 'Content-Type': 'application/json' },
                body: JSON.stringify(settings)
            });
            
            if (response.ok) {
                alert('settings saved');
                this.settingsCache = settings;
            } else {
                alert('save failed');
            }
        } catch (error) {
            alert('save error: ' + error.message);
        }
    }
    
    resetSettings() {
        if (confirm('reset all settings to defaults?')) {
            document.getElementById('silentLogging').checked = false;
            document.getElementById('scanInterval').value = 1000;
            document.getElementById('entropyThreshold').value = 50;
            document.getElementById('entropyMonitoring').checked = true;
            document.getElementById('supabaseUrl').value = '';
            document.getElementById('supabaseKey').value = '';
            
            this.saveSettings();
        }
    }
    
    async refreshLogs() {
        try {
            const response = await fetch('/api/logs');
            const data = await response.json();
            
            document.getElementById('logCount').textContent = `${data.count} entries`;
            document.getElementById('logStatus').textContent = data.silent_logging ? 'logging active' : 'manual only';
            
            this.displayLogs(data.logs);
        } catch (error) {
            document.getElementById('logStatus').textContent = 'error loading logs';
            console.error('[PN2133] failed to refresh logs:', error);
        }
    }
    
    displayLogs(logs) {
        const logsDiv = document.getElementById('logsDisplay');
        
        if (!logs || logs.length === 0) {
            logsDiv.innerHTML = '<div class="placeholder-text">[ No log entries available ]</div>';
            return;
        }
        
        let html = '';
        logs.reverse().forEach(log => {
            const timestamp = new Date(parseInt(log.timestamp));
            html += `
                <div class="log-entry">
                    <div class="log-uid">${log.uid}</div>
                    <div class="log-meta">
                        Type: ${log.tag_type} | 
                        Time: ${timestamp.toLocaleString()}
                    </div>`;
            
            if (log.data) {
                html += `<div class="log-data">Data: ${log.data}</div>`;
            }
            
            html += '</div>';
        });
        
        logsDiv.innerHTML = html;
    }
    
    async exportLogs() {
        try {
            const response = await fetch('/api/logs');
            const data = await response.json();
            
            const exportData = {
                exported_at: new Date().toISOString(),
                device_id: 'PN2133',
                log_count: data.count,
                logs: data.logs
            };
            
            const blob = new Blob([JSON.stringify(exportData, null, 2)], 
                { type: 'application/json' });
            const url = URL.createObjectURL(blob);
            
            const a = document.createElement('a');
            a.href = url;
            a.download = `pn2133_logs_${Date.now()}.json`;
            document.body.appendChild(a);
            a.click();
            document.body.removeChild(a);
            URL.revokeObjectURL(url);
            
        } catch (error) {
            alert('export failed: ' + error.message);
        }
    }
    
    async clearLogs() {
        if (confirm('clear all log entries? this cannot be undone.')) {
            try {
                const response = await fetch('/api/clear', { method: 'POST' });
                if (response.ok) {
                    this.refreshLogs();
                } else {
                    alert('clear failed');
                }
            } catch (error) {
                alert('clear error: ' + error.message);
            }
        }
    }
    
    startSystemClock() {
        setInterval(() => {
            const now = new Date();
            document.getElementById('systemTime').textContent = 
                now.toLocaleTimeString('en-US', { hour12: false });
        }, 1000);
    }
    
    destroy() {
        if (this.entropyInterval) clearInterval(this.entropyInterval);
        if (this.logsInterval) clearInterval(this.logsInterval);
    }
}

// system initialization 
document.addEventListener('DOMContentLoaded', () => {
    window.pn2133 = new PN2133Interface();
    
    // handle page unload
    window.addEventListener('beforeunload', () => {
        if (window.pn2133) {
            window.pn2133.destroy();
        }
    });
});

// keyboard shortcuts for quick access
document.addEventListener('keydown', (e) => {
    if (e.ctrlKey || e.metaKey) {
        switch (e.key) {
            case '1':
                e.preventDefault();
                window.pn2133.switchTab('scanner');
                break;
            case '2':
                e.preventDefault();
                window.pn2133.switchTab('logger');
                break;
            case '3':
                e.preventDefault();
                window.pn2133.switchTab('settings');
                break;
            case '4':
                e.preventDefault();
                window.pn2133.switchTab('data');
                break;
            case 's':
                e.preventDefault();
                if (window.pn2133.activeTab === 'scanner') {
                    window.pn2133.performManualScan();
                }
                break;
        }
    }
});

// diagnostic functions for field troubleshooting
window.pn2133_diag = {
    checkAPI: async function() {
        try {
            const response = await fetch('/api/settings');
            console.log('API Status:', response.ok ? 'OK' : 'FAILED');
            return response.ok;
        } catch (error) {
            console.error('API Error:', error);
            return false;
        }
    },
    
    testScan: async function() {
        console.log('triggering manual scan...');
        return window.pn2133.performManualScan();
    },
    
    dumpLogs: async function() {
        try {
            const response = await fetch('/api/logs');
            const data = await response.json();
            console.log('Current logs:', data);
            return data;
        } catch (error) {
            console.error('Log dump failed:', error);
            return null;
        }
    }
};

console.log('[PN2133] diagnostics available via window.pn2133_diag');
console.log('[PN2133] keyboard shortcuts: ctrl+1-4 for tabs, ctrl+s for scan');