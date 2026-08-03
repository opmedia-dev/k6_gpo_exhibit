import { useState, useRef, useEffect } from "react"
import { Card, CardContent, CardHeader, CardTitle, CardDescription } from "@/components/ui/card"
import { Button } from "@/components/ui/button"
import { Input } from "@/components/ui/input"
import { Slider } from "@/components/ui/slider"
import { Select, SelectContent, SelectItem, SelectTrigger, SelectValue } from "@/components/ui/select"
import { Table, TableBody, TableCell, TableRow } from "@/components/ui/table"
import { Badge } from "@/components/ui/badge"
import { Volume2, Bell, Clock, Wifi, Coins, Folder, FileAudio, PlayCircle, Trash2, Upload, Mic, Disc, Save, FolderPlus, DownloadCloud, Phone } from "lucide-react"
import { Progress } from "@/components/ui/progress"

interface FileEntry { name: string; size: number; dir: boolean }

function post(url: string) { return fetch(url, { method: 'POST' }).catch(() => {}) }

export function SettingsTab() {
  // Sound
  const [vol, setVol] = useState([15])
  const [lineLevel, setLineLevel] = useState([100])
  const [bellFreq, setBellFreq] = useState("25")
  // Auto ring
  const [arMin, setArMin] = useState("5")
  const [arMax, setArMax] = useState("30")
  const [rtMin, setRtMin] = useState("4")
  const [rtMax, setRtMax] = useState("8")
  const [ringMax, setRingMax] = useState("10")
  const [alertIdle, setAlertIdle] = useState("120")
  // Dialling
  const [digitGap, setDigitGap] = useState("3")
  // Wifi
  const [wifiMode, setWifiMode] = useState("ap")
  const [ssid, setSsid] = useState("")
  const [wifiPass, setWifiPass] = useState("")
  const [wifiStatus, setWifiStatus] = useState("")
  // Coin
  const [coinMode, setCoinModeVal] = useState("-1")
  const [coinActive, setCoinActive] = useState(false)
  // Files
  const [cwd, setCwd] = useState("/")
  const [files, setFiles] = useState<FileEntry[]>([])
  const [upStatus, setUpStatus] = useState("")
  // Recording
  const [isRecording, setIsRecording] = useState(false)
  const [recordTime, setRecordTime] = useState(0)
  const [recBlob, setRecBlob] = useState<Blob | null>(null)
  const [recName, setRecName] = useState("")
  const [recDir, setRecDir] = useState("/history/")
  const [recStatus, setRecStatus] = useState("")
  const timerRef = useRef<ReturnType<typeof setInterval> | null>(null)
  const mediaRef = useRef<MediaRecorder | null>(null)
  const chunksRef = useRef<BlobPart[]>([])
  const audioRef = useRef<HTMLAudioElement | null>(null)
  // OTA
  const [otaProgress, setOtaProgress] = useState(0)
  const [otaStatus, setOtaStatus] = useState("")
  const [otaBusy, setOtaBusy] = useState(false)
  const otaRef = useRef<HTMLInputElement>(null)
  const upRef = useRef<HTMLInputElement>(null)
  // Init flag for wifi
  const wifiInitRef = useRef(false)
  const uiEditRef = useRef(0)

  // Load current values from /api/status
  useEffect(() => {
    fetch('/api/status').then(r => r.json()).then(d => {
      if (d.volume !== undefined) setVol([d.volume])
      if (d.bell_freq !== undefined) setBellFreq(String(d.bell_freq))
      if (d.digit_gap !== undefined) setDigitGap(String(Math.round(d.digit_gap / 1000)))
      if (d.line_level !== undefined) setLineLevel([d.line_level])
      if (d.ring_max !== undefined) setRingMax(String(d.ring_max))
      if (d.rt_min !== undefined) { setRtMin(String(d.rt_min)); setRtMax(String(d.rt_max)) }
      if (d.alert_idle !== undefined) setAlertIdle(String(d.alert_idle))
      if (d.ar_min !== undefined) { setArMin(String(Math.round(d.ar_min / 60000))); setArMax(String(Math.round(d.ar_max / 60000))) }
      if (d.coin_override !== undefined) { setCoinModeVal(String(d.coin_override)); setCoinActive(!!d.coin_active) }
      if (!wifiInitRef.current && d.wifi_cfg_mode !== undefined) {
        setWifiMode(d.wifi_cfg_mode)
        if (d.wifi_cfg_ssid) setSsid(d.wifi_cfg_ssid)
        wifiInitRef.current = true
      }
    }).catch(() => {})
    loadFiles("/")
  }, [])

  // Debounced volume
  const volTimer = useRef<ReturnType<typeof setTimeout> | null>(null)
  const handleVol = (v: number[]) => {
    setVol(v); uiEditRef.current = Date.now()
    if (volTimer.current) clearTimeout(volTimer.current)
    volTimer.current = setTimeout(() => post('/api/volume?v=' + v[0]), 150)
  }

  // Debounced line level
  const llTimer = useRef<ReturnType<typeof setTimeout> | null>(null)
  const handleLineLevel = (v: number[]) => {
    setLineLevel(v); uiEditRef.current = Date.now()
    if (llTimer.current) clearTimeout(llTimer.current)
    llTimer.current = setTimeout(() => post('/api/linelevel?v=' + v[0]), 150)
  }

  // Files
  const loadFiles = (path: string) => {
    setCwd(path)
    fetch('/api/files?path=' + encodeURIComponent(path))
      .then(r => r.json())
      .then((d: FileEntry[]) => {
        setFiles((d || []).sort((a, b) => (b.dir ? 1 : 0) - (a.dir ? 1 : 0) || a.name.localeCompare(b.name)))
      }).catch(() => {})
  }

  const navUp = () => {
    const parts = cwd.split('/').filter(Boolean)
    parts.pop()
    loadFiles('/' + parts.join('/') + (parts.length ? '/' : ''))
  }

  const delFile = (name: string) => {
    if (!confirm('Delete ' + name + '?')) return
    fetch('/api/delete?path=' + encodeURIComponent(cwd + name), { method: 'POST' })
      .then(r => r.json())
      .then(d => { setUpStatus(d.ok ? 'Deleted' : (d.error ?? 'Error')); loadFiles(cwd) })
      .catch(() => {})
  }

  const playFile = (name: string) => {
    if (audioRef.current) { audioRef.current.pause(); audioRef.current = null }
    const a = new Audio('/api/preview?path=' + encodeURIComponent(cwd + name))
    audioRef.current = a; a.play().catch(() => {})
  }

  const mkdirPrompt = () => {
    const n = prompt('Folder name:'); if (!n) return
    fetch('/api/mkdir?path=' + encodeURIComponent(cwd + n), { method: 'POST' })
      .then(r => r.json()).then(d => { setUpStatus(d.ok ? 'Created' : (d.error ?? 'Error')); loadFiles(cwd) }).catch(() => {})
  }

  const upload = () => {
    const f = upRef.current?.files
    if (!f || !f.length) return
    setUpStatus('Uploading...')
    let done = 0; const errs: string[] = []
    Array.from(f).forEach(file => {
      const fd = new FormData(); fd.append('file', file)
      fetch('/api/upload?path=' + encodeURIComponent(cwd), { method: 'POST', body: fd })
        .then(r => r.json()).then(d => {
          done++; if (!d.ok) errs.push(file.name + ': ' + (d.error ?? 'failed'))
          if (done === f.length) { setUpStatus(errs.length ? errs.join(', ') : 'Upload complete'); if (upRef.current) upRef.current.value = ''; loadFiles(cwd) }
        }).catch(() => { done++; setUpStatus('Upload failed') })
    })
  }

  // Recording
  const startRec = () => {
    navigator.mediaDevices.getUserMedia({ audio: true }).then(stream => {
      chunksRef.current = []
      const mr = new MediaRecorder(stream, { mimeType: 'audio/webm;codecs=opus' })
      mr.ondataavailable = e => { if (e.data.size > 0) chunksRef.current.push(e.data) }
      mr.onstop = () => {
        stream.getTracks().forEach(t => t.stop())
        const blob = new Blob(chunksRef.current, { type: 'audio/webm' })
        setRecBlob(blob)
        setRecStatus('Recording complete. Listen back, then save or discard.')
      }
      mr.start(100); mediaRef.current = mr
      setIsRecording(true); setRecordTime(0); setRecStatus('')
      timerRef.current = setInterval(() => setRecordTime(t => t + 1), 1000)
    }).catch(() => setRecStatus('Microphone access denied. Please allow microphone access and try again.'))
  }

  const stopRec = () => {
    if (mediaRef.current && mediaRef.current.state !== 'inactive') mediaRef.current.stop()
    if (timerRef.current) clearInterval(timerRef.current)
    setIsRecording(false)
  }

  const saveRec = () => {
    if (!recBlob || !recName) { setRecStatus('Please enter a filename.'); return }
    const fd = new FormData(); fd.append('file', recBlob, recName + '.mp3')
    setRecStatus('Saving...')
    fetch('/api/upload?path=' + encodeURIComponent(recDir), { method: 'POST', body: fd })
      .then(r => r.json()).then(d => {
        setRecStatus(d.ok ? 'Saved to ' + recDir + recName + '.mp3' : (d.error ?? 'Error'))
        if (d.ok) { setRecBlob(null); setRecName(''); setRecordTime(0); loadFiles(cwd) }
      }).catch(() => setRecStatus('Save failed'))
  }

  const fmtTime = (s: number) => `${Math.floor(s / 60)}:${(s % 60).toString().padStart(2, '0')}`

  // OTA
  const otaUpload = () => {
    const f = otaRef.current?.files?.[0]; if (!f) return
    setOtaBusy(true); setOtaProgress(0); setOtaStatus('Uploading firmware… do not disconnect.')
    const xhr = new XMLHttpRequest(); xhr.open('POST', '/api/ota')
    xhr.upload.onprogress = e => { if (e.lengthComputable) setOtaProgress(Math.round(e.loaded / e.total * 100)) }
    xhr.onload = () => {
      const d = JSON.parse(xhr.responseText)
      if (d.ok) { setOtaStatus('Firmware updated! Rebooting…'); setTimeout(() => location.reload(), 8000) }
      else { setOtaStatus('Error: ' + d.error); setOtaBusy(false) }
    }
    xhr.onerror = () => { setOtaStatus('Upload failed'); setOtaBusy(false) }
    const fd = new FormData(); fd.append('firmware', f); xhr.send(fd)
  }

  const rollback = () => {
    if (!confirm('Restore the previous firmware version? The telephone will restart.')) return
    fetch('/api/rollback', { method: 'POST' }).then(r => r.json()).then(d => {
      if (d.ok) { setOtaStatus('Restoring previous version… restarting'); setTimeout(() => location.reload(), 8000) }
      else setOtaStatus('Error: ' + (d.error ?? 'No previous firmware available'))
    }).catch(() => {})
  }

  const saveWifi = () => {
    if (wifiMode === 'sta' && !ssid) { alert('Enter the Wi-Fi network name.'); return }
    const msg = wifiMode === 'sta'
      ? `The telephone will restart and try to join "${ssid}". If it can't connect it falls back to K6-Exhibit hotspot.`
      : 'The telephone will restart and host its own K6-Exhibit hotspot.'
    if (!confirm(msg)) return
    setWifiStatus('Saving and restarting…')
    fetch('/api/wifi?mode=' + wifiMode + '&ssid=' + encodeURIComponent(ssid) + '&pass=' + encodeURIComponent(wifiPass), { method: 'POST' })
      .then(() => { setTimeout(() => location.reload(), 9000) })
      .catch(() => setWifiStatus('Restarting… reconnect to the telephone network.'))
  }

  // Breadcrumb
  const parts = cwd.split('/').filter(Boolean)

  return (
    <div className="space-y-6">
      {/* SOUND SETTINGS */}
      <Card>
        <CardHeader className="pb-3 border-b border-border/50">
          <CardTitle className="text-lg flex items-center gap-2"><Volume2 className="w-5 h-5 text-primary" /> Sound Settings</CardTitle>
        </CardHeader>
        <CardContent className="pt-6 space-y-6">
          <div className="space-y-3">
            <div className="flex justify-between">
              <label className="text-sm font-medium">Handset Volume</label>
              <span className="text-sm font-mono text-muted-foreground">{vol[0]} / 21</span>
            </div>
            <Slider value={vol} onValueChange={handleVol} max={21} step={1} />
          </div>
          <div className="space-y-3 pt-2">
            <label className="text-sm font-medium">Bell Frequency (Hz)</label>
            <div className="flex gap-3 flex-wrap">
              <Input type="number" value={bellFreq} onChange={e => setBellFreq(e.target.value)} className="w-24" />
              <Button variant="secondary" onClick={() => post('/api/bellfreq?hz=' + bellFreq)}><Save className="w-4 h-4 mr-2" /> Save</Button>
              <Button variant="outline" onClick={() => post('/api/testring?secs=3')}><Bell className="w-4 h-4 mr-2" /> Test Ring (3s)</Button>
            </div>
          </div>
          <div className="space-y-3 pt-2 border-t border-border/50">
            <div className="flex justify-between">
              <label className="text-sm font-medium">Line Level (earpiece trim)</label>
              <span className="text-sm font-mono text-muted-foreground">{lineLevel[0]}%</span>
            </div>
            <Slider value={lineLevel} onValueChange={handleLineLevel} max={100} step={1} />
            <Button variant="outline" size="sm" onClick={() => post('/api/tone?hz=1000&secs=5')}>Test Tone (5s)</Button>
          </div>
        </CardContent>
      </Card>

      {/* AUTOMATIC RINGING */}
      <Card>
        <CardHeader className="pb-3 border-b border-border/50">
          <CardTitle className="text-lg flex items-center gap-2"><Bell className="w-5 h-5 text-primary" /> Automatic Ringing</CardTitle>
        </CardHeader>
        <CardContent className="pt-6 space-y-5">
          <div>
            <label className="text-sm font-medium block mb-2">Time Between Rings (minutes)</label>
            <div className="flex items-center gap-2 flex-wrap">
              <span className="text-sm text-muted-foreground">Every</span>
              <Input type="number" value={arMin} onChange={e => setArMin(e.target.value)} className="w-20" />
              <span className="text-sm text-muted-foreground">to</span>
              <Input type="number" value={arMax} onChange={e => setArMax(e.target.value)} className="w-20" />
              <span className="text-sm text-muted-foreground">minutes</span>
              <Button variant="secondary" onClick={() => post('/api/autoring?min=' + (+arMin * 60000) + '&max=' + (+arMax * 60000))}><Save className="w-4 h-4" /></Button>
            </div>
          </div>
          <div>
            <label className="text-sm font-medium block mb-2">How Long to Ring (seconds)</label>
            <div className="flex items-center gap-2 flex-wrap">
              <Input type="number" value={rtMin} onChange={e => setRtMin(e.target.value)} className="w-20" />
              <span className="text-sm text-muted-foreground">to</span>
              <Input type="number" value={rtMax} onChange={e => setRtMax(e.target.value)} className="w-20" />
              <span className="text-sm text-muted-foreground">seconds</span>
              <Button variant="secondary" onClick={() => post('/api/ringtone?min=' + rtMin + '&max=' + rtMax)}><Save className="w-4 h-4" /></Button>
            </div>
          </div>
          <div>
            <label className="text-sm font-medium block mb-2">Maximum Ring Cycles (0 = unlimited)</label>
            <div className="flex items-center gap-2 flex-wrap">
              <Input type="number" value={ringMax} onChange={e => setRingMax(e.target.value)} className="w-24" />
              <span className="text-sm text-muted-foreground">cycles</span>
              <Button variant="secondary" onClick={() => post('/api/ringcount?n=' + ringMax)}><Save className="w-4 h-4" /></Button>
            </div>
          </div>
          <div className="border-t border-border/50 pt-4">
            <label className="text-sm font-medium block mb-2">Inactivity Warning (minutes, 0 to disable)</label>
            <div className="flex items-center gap-2 flex-wrap">
              <span className="text-sm text-muted-foreground">Warn after</span>
              <Input type="number" value={alertIdle} onChange={e => setAlertIdle(e.target.value)} className="w-24" />
              <span className="text-sm text-muted-foreground">minutes</span>
              <Button variant="secondary" onClick={() => post('/api/alertidle?v=' + alertIdle)}><Save className="w-4 h-4" /></Button>
            </div>
          </div>
        </CardContent>
      </Card>

      {/* DIALLING */}
      <Card>
        <CardHeader className="pb-3 border-b border-border/50">
          <CardTitle className="text-lg flex items-center gap-2"><Phone className="w-5 h-5 text-primary" /> Dialling</CardTitle>
        </CardHeader>
        <CardContent className="pt-6">
          <label className="text-sm font-medium block mb-2">Time Allowed Between Digits (seconds)</label>
          <div className="flex items-center gap-2 flex-wrap">
            <span className="text-sm text-muted-foreground">Wait</span>
            <Input type="number" value={digitGap} onChange={e => setDigitGap(e.target.value)} className="w-20" />
            <span className="text-sm text-muted-foreground">seconds</span>
            <Button variant="secondary" onClick={() => post('/api/digitgap?v=' + (+digitGap * 1000))}><Save className="w-4 h-4" /></Button>
          </div>
        </CardContent>
      </Card>

      {/* WIFI */}
      <Card>
        <CardHeader className="pb-3 border-b border-border/50">
          <CardTitle className="text-lg flex items-center gap-2"><Wifi className="w-5 h-5 text-primary" /> Wi-Fi Network</CardTitle>
          <CardDescription>Changing this restarts the telephone</CardDescription>
        </CardHeader>
        <CardContent className="pt-6 space-y-4">
          <div className="space-y-2">
            <label className="text-sm font-medium">Connection Mode</label>
            <Select value={wifiMode} onValueChange={v => { setWifiMode(v) }}>
              <SelectTrigger><SelectValue /></SelectTrigger>
              <SelectContent>
                <SelectItem value="ap">Host its own hotspot (K6-Exhibit)</SelectItem>
                <SelectItem value="sta">Join an existing Wi-Fi network</SelectItem>
              </SelectContent>
            </Select>
          </div>
          {wifiMode === 'sta' && (
            <>
              <div className="space-y-2">
                <label className="text-sm font-medium">Network Name (SSID)</label>
                <Input placeholder="Your Wi-Fi name" value={ssid} onChange={e => setSsid(e.target.value)} />
              </div>
              <div className="space-y-2">
                <label className="text-sm font-medium">Password</label>
                <Input type="password" placeholder="Leave blank for open network" value={wifiPass} onChange={e => setWifiPass(e.target.value)} />
              </div>
              <p className="text-xs text-muted-foreground">If the telephone can't join this network it automatically falls back to hosting its own K6-Exhibit hotspot.</p>
            </>
          )}
          <Button variant="destructive" className="w-full" onClick={saveWifi}>Save &amp; Restart</Button>
          {wifiStatus && <p className="text-sm text-amber-500">{wifiStatus}</p>}
        </CardContent>
      </Card>

      {/* COIN BOX */}
      <Card>
        <CardHeader className="pb-3 border-b border-border/50">
          <CardTitle className="text-lg flex items-center gap-2"><Coins className="w-5 h-5 text-primary" /> A+B Coin Box</CardTitle>
        </CardHeader>
        <CardContent className="pt-6 space-y-4">
          <Select value={coinMode} onValueChange={v => {
            setCoinModeVal(v)
            fetch('/api/coinmode?v=' + v, { method: 'POST' }).then(r => r.json()).then(d => setCoinActive(!!d.active)).catch(() => {})
          }}>
            <SelectTrigger><SelectValue /></SelectTrigger>
            <SelectContent>
              <SelectItem value="-1">Auto (detect at boot)</SelectItem>
              <SelectItem value="0">Force Off</SelectItem>
              <SelectItem value="1">Force On</SelectItem>
            </SelectContent>
          </Select>
          <div className="flex items-center justify-between bg-muted/30 p-3 rounded text-sm">
            <span className="text-muted-foreground">Hardware Status</span>
            <Badge variant="outline">{coinActive ? 'ACTIVE' : 'DISCONNECTED'}</Badge>
          </div>
        </CardContent>
      </Card>

      {/* AUDIO FILES */}
      <Card>
        <CardHeader className="pb-3 border-b border-border/50">
          <div className="flex flex-wrap items-center justify-between gap-3">
            <CardTitle className="text-lg flex items-center gap-2"><Folder className="w-5 h-5 text-primary shrink-0" /> Audio Files</CardTitle>
            <div className="flex gap-2 shrink-0">
              <Button variant="outline" size="sm" className="hidden sm:flex" onClick={mkdirPrompt}><FolderPlus className="w-4 h-4 mr-2" /> New Folder</Button>
              <Button variant="secondary" size="sm" onClick={() => upRef.current?.click()}><Upload className="w-4 h-4 mr-2" /> Upload</Button>
              <input ref={upRef} type="file" multiple accept=".mp3,.MP3,.wav,.WAV" className="hidden" onChange={upload} />
            </div>
          </div>
        </CardHeader>
        <CardContent className="p-0">
          {/* Breadcrumb */}
          <div className="bg-muted/30 px-4 py-2 border-b border-border/50 text-sm font-mono text-muted-foreground flex items-center gap-1 flex-wrap">
            <span className="cursor-pointer hover:text-foreground" onClick={() => loadFiles('/')}>/ </span>
            {parts.map((p, i) => {
              const path = '/' + parts.slice(0, i + 1).join('/') + '/'
              return <span key={i} className="cursor-pointer hover:text-foreground" onClick={() => loadFiles(path)}>{p}/ </span>
            })}
          </div>
          <Table>
            <TableBody>
              {cwd !== '/' && (
                <TableRow className="cursor-pointer hover:bg-muted/30" onClick={navUp}>
                  <TableCell className="w-[30px]"><Folder className="w-4 h-4 text-muted-foreground" /></TableCell>
                  <TableCell className="font-mono text-sm">..</TableCell>
                  <TableCell></TableCell>
                  <TableCell></TableCell>
                </TableRow>
              )}
              {files.map((f, i) => {
                const sz = f.size < 1024 ? f.size + 'B' : f.size < 1048576 ? (f.size / 1024).toFixed(1) + 'KB' : (f.size / 1048576).toFixed(1) + 'MB'
                const playable = !f.dir && (f.name.toLowerCase().endsWith('.mp3') || f.name.toLowerCase().endsWith('.wav'))
                return (
                  <TableRow key={i}>
                    <TableCell className="w-[30px]">
                      {f.dir ? <Folder className="w-4 h-4 text-amber-500" /> : <FileAudio className="w-4 h-4 text-muted-foreground" />}
                    </TableCell>
                    <TableCell className="font-mono text-sm cursor-pointer" onClick={() => f.dir && loadFiles(cwd + f.name + '/')}>{f.name}{f.dir ? '/' : ''}</TableCell>
                    <TableCell className="text-xs text-muted-foreground text-right">{!f.dir && sz}</TableCell>
                    <TableCell className="text-right">
                      <div className="flex justify-end gap-1">
                        {playable && <Button variant="ghost" size="icon" className="h-8 w-8" onClick={() => playFile(f.name)}><PlayCircle className="w-4 h-4" /></Button>}
                        {!f.dir && <Button variant="ghost" size="icon" className="h-8 w-8 text-destructive" onClick={() => delFile(f.name)}><Trash2 className="w-4 h-4" /></Button>}
                      </div>
                    </TableCell>
                  </TableRow>
                )
              })}
              {files.length === 0 && cwd === '/' && (
                <TableRow><TableCell colSpan={4} className="text-center text-muted-foreground text-sm py-6">SD card empty or not mounted</TableCell></TableRow>
              )}
            </TableBody>
          </Table>
          {upStatus && <p className="px-4 py-2 text-sm text-muted-foreground border-t border-border/50">{upStatus}</p>}
        </CardContent>
      </Card>

      {/* RECORD AUDIO */}
      <Card>
        <CardHeader className="pb-3 border-b border-border/50">
          <CardTitle className="text-lg flex items-center gap-2"><Mic className="w-5 h-5 text-primary" /> Record Audio</CardTitle>
          <CardDescription>Record directly from your device microphone</CardDescription>
        </CardHeader>
        <CardContent className="pt-6 text-center space-y-4">
          <div className="font-mono text-3xl tabular-nums">{fmtTime(recordTime)}</div>
          <div className="flex justify-center gap-4">
            {!isRecording ? (
              <Button size="lg" variant="default" className="w-32 rounded-full h-14" onClick={startRec} disabled={!!recBlob}>Record</Button>
            ) : (
              <Button size="lg" variant="destructive" className="w-32 rounded-full h-14 animate-pulse" onClick={stopRec}>Stop</Button>
            )}
          </div>
          {recBlob && !isRecording && (
            <div className="p-4 bg-muted rounded-lg space-y-3 text-left">
              <audio controls src={URL.createObjectURL(recBlob)} className="w-full" />
              <div className="flex gap-2 flex-wrap items-end">
                <div className="flex-1 min-w-[120px] space-y-1">
                  <label className="text-xs text-muted-foreground">Save as filename</label>
                  <Input placeholder="filename (no extension)" value={recName} onChange={e => setRecName(e.target.value)} />
                </div>
                <div className="space-y-1">
                  <label className="text-xs text-muted-foreground">Folder</label>
                  <Select value={recDir} onValueChange={setRecDir}>
                    <SelectTrigger className="w-32"><SelectValue /></SelectTrigger>
                    <SelectContent>
                      <SelectItem value="/history/">/history/</SelectItem>
                      <SelectItem value="/numbers/">/numbers/</SelectItem>
                      <SelectItem value="/system/">/system/</SelectItem>
                    </SelectContent>
                  </Select>
                </div>
              </div>
              <div className="flex gap-2">
                <Button onClick={saveRec} disabled={!recName}>Save to SD</Button>
                <Button variant="outline" onClick={() => { setRecBlob(null); setRecordTime(0); setRecName('') }}>Discard</Button>
              </div>
            </div>
          )}
          {recStatus && <p className="text-sm text-muted-foreground">{recStatus}</p>}
        </CardContent>
      </Card>

      {/* FIRMWARE */}
      <Card>
        <CardHeader className="pb-3 border-b border-border/50">
          <CardTitle className="text-lg flex items-center gap-2"><Disc className="w-5 h-5 text-primary" /> Firmware Update</CardTitle>
        </CardHeader>
        <CardContent className="pt-6 space-y-4">
          <div
            className="p-4 border border-dashed rounded-lg text-center cursor-pointer hover:bg-muted/50 transition-colors"
            onClick={() => !otaBusy && otaRef.current?.click()}
          >
            <DownloadCloud className="w-8 h-8 text-muted-foreground mx-auto mb-2" />
            <p className="text-sm font-medium">{otaRef.current?.files?.[0]?.name ?? 'Select .bin firmware file'}</p>
            <input ref={otaRef} type="file" accept=".bin" className="hidden" onChange={() => setOtaStatus('')} />
          </div>
          {otaProgress > 0 && <Progress value={otaProgress} className="h-2" />}
          {otaStatus && <p className="text-sm text-muted-foreground">{otaStatus}</p>}
          <Button className="w-full" onClick={otaUpload} disabled={otaBusy}>Install Update</Button>
          <div className="border-t border-border/50 pt-4">
            <Button variant="outline" className="w-full text-destructive hover:bg-destructive/10" onClick={rollback} disabled={otaBusy}>
              Restore Previous Version
            </Button>
          </div>
        </CardContent>
      </Card>
    </div>
  )
}
