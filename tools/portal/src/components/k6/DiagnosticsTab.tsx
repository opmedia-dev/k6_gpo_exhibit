import { useEffect, useState } from "react"
import { Card, CardContent, CardHeader, CardTitle, CardDescription } from "@/components/ui/card"
import { Button } from "@/components/ui/button"
import { Table, TableBody, TableCell, TableHead, TableHeader, TableRow } from "@/components/ui/table"
import { Badge } from "@/components/ui/badge"
import { AlertTriangle, Activity, Stethoscope, RefreshCw, Cpu, Wifi, Database } from "lucide-react"

interface ErrorEntry { time: number; type: string; detail?: string }
interface BootLine { boot: number; raw: number }
interface DiagData { boot_lines?: BootLine[]; selftest?: string; selftest_uptime?: number }
interface StatusData {
  heap?: number; sd?: boolean; sd_total?: number; sd_used?: number
  uptime?: number; firmware?: string; wifi_mode?: string; wifi_ssid?: string; wifi_ip?: string
  line_cal?: boolean
}

export function DiagnosticsTab() {
  const [errors, setErrors] = useState<ErrorEntry[]>([])
  const [diag, setDiag] = useState<DiagData>({})
  const [logText, setLogText] = useState('Select a log to view.')
  const [curLog, setCurLog] = useState('system')
  const [sysInfo, setSysInfo] = useState<StatusData>({})

  useEffect(() => {
    fetch('/api/diagnostics').then(r => r.json()).then(d => setErrors(d || [])).catch(() => {})
    fetch('/api/diag').then(r => r.json()).then(setDiag).catch(() => {})
    fetch('/api/status').then(r => r.json()).then(setSysInfo).catch(() => {})
    loadLog('system')
  }, [])

  const loadLog = (which: string) => {
    setCurLog(which)
    setLogText('Loading...')
    fetch('/api/logs/' + which)
      .then(r => r.text())
      .then(t => setLogText(t || '(empty)'))
      .catch(() => setLogText('Failed to load log.'))
  }

  const clearLog = () => {
    if (!confirm('Clear ' + curLog + ' log?')) return
    fetch('/api/logs/clear?log=' + curLog, { method: 'POST' }).then(() => loadLog(curLog)).catch(() => {})
  }

  const reboot = () => {
    if (!confirm('This will restart the telephone. Any active calls will be disconnected.')) return
    fetch('/api/reboot', { method: 'POST' }).then(() => {
      setSysInfo(prev => ({ ...prev, _rebooting: true } as StatusData))
    }).catch(() => {})
  }

  const heapKB = Math.round((sysInfo.heap ?? 0) / 1024)
  const uH = Math.floor((sysInfo.uptime ?? 0) / 3600)
  const uM = Math.floor(((sysInfo.uptime ?? 0) % 3600) / 60)

  const stUptime = diag.selftest_uptime ?? 0
  const stH = Math.floor(stUptime / 3600)
  const stM = Math.floor((stUptime % 3600) / 60)

  return (
    <div className="space-y-6">
      <Card>
        <CardHeader className="pb-3 border-b border-border/50">
          <CardTitle className="text-lg flex items-center gap-2 text-destructive">
            <AlertTriangle className="w-5 h-5" />
            Error Log
          </CardTitle>
          <CardDescription>Hardware and system errors since last power-on</CardDescription>
        </CardHeader>
        <CardContent className="p-0">
          {errors.length === 0 ? (
            <div className="p-6 text-center text-muted-foreground text-sm">No errors recorded since last power-on.</div>
          ) : (
            <Table>
              <TableHeader>
                <TableRow>
                  <TableHead className="w-[90px]">Time</TableHead>
                  <TableHead>Type</TableHead>
                  <TableHead>Detail</TableHead>
                </TableRow>
              </TableHeader>
              <TableBody>
                {errors.map((err, i) => {
                  const mins = Math.floor(err.time / 60)
                  const secs = err.time % 60
                  const isSd = err.type === 'SD_FAILURE'
                  return (
                    <TableRow key={i}>
                      <TableCell className="font-mono text-muted-foreground text-xs">{mins}m {secs}s</TableCell>
                      <TableCell>
                        <Badge variant="outline" className={`font-mono text-xs ${isSd ? 'text-destructive border-destructive/30' : 'text-amber-500 border-amber-500/30'}`}>
                          {err.type}
                        </Badge>
                      </TableCell>
                      <TableCell className="text-sm">{err.detail ?? '—'}</TableCell>
                    </TableRow>
                  )
                })}
              </TableBody>
            </Table>
          )}
        </CardContent>
      </Card>

      <Card>
        <CardHeader className="pb-3 border-b border-border/50">
          <CardTitle className="text-lg flex items-center gap-2 mb-3">
            <Activity className="w-5 h-5 text-primary" />
            Activity Log
          </CardTitle>
          <div className="flex flex-wrap gap-2">
            <Button variant={curLog === 'system' ? 'default' : 'outline'} size="sm" onClick={() => loadLog('system')}>System Events</Button>
            <Button variant={curLog === 'calls' ? 'default' : 'outline'} size="sm" onClick={() => loadLog('calls')}>Call History</Button>
            <Button variant="ghost" size="sm" className="text-muted-foreground" onClick={clearLog}>Clear</Button>
          </div>
        </CardHeader>
        <CardContent className="p-0">
          <pre className="p-4 text-xs font-mono text-muted-foreground bg-[#0a0a0a] overflow-x-auto m-0 max-h-[300px] overflow-y-auto whitespace-pre-wrap">
            {logText}
          </pre>
        </CardContent>
      </Card>

      <div className="grid grid-cols-1 md:grid-cols-2 gap-6">
        <Card>
          <CardHeader className="pb-3 border-b border-border/50">
            <CardTitle className="text-lg flex items-center gap-2">
              <Stethoscope className="w-5 h-5 text-primary" />
              Last Self-Test
            </CardTitle>
            {diag.selftest && <CardDescription>Ran at uptime {stH}h {stM}m</CardDescription>}
          </CardHeader>
          <CardContent className="p-0">
            <pre className="p-4 text-xs font-mono bg-[#0a0a0a] overflow-x-auto m-0 h-[200px] overflow-y-auto whitespace-pre-wrap">
              {diag.selftest ? diag.selftest.split('\n').map((line, i) => (
                <div key={i} className={
                  line.includes("WARN") ? "text-amber-400" :
                  line.includes("FAIL") ? "text-destructive" :
                  (line.includes("OK") || line.includes("PASS")) ? "text-emerald-400" :
                  "text-muted-foreground"
                }>{line}</div>
              )) : (
                <span className="text-muted-foreground">No self-test has been run yet. Run one from the Terminal tab.</span>
              )}
            </pre>
          </CardContent>
        </Card>

        <Card>
          <CardHeader className="pb-3 border-b border-border/50">
            <CardTitle className="text-lg flex items-center gap-2">
              <Database className="w-5 h-5 text-primary" />
              Line-Sense Drift
            </CardTitle>
            <CardDescription>On-hook ADC readings at each boot</CardDescription>
          </CardHeader>
          <CardContent className="p-0">
            {!diag.boot_lines?.length ? (
              <div className="p-6 text-center text-muted-foreground text-sm">No boot readings recorded yet.</div>
            ) : (
              <Table>
                <TableHeader>
                  <TableRow>
                    <TableHead>Boot #</TableHead>
                    <TableHead className="text-right">On-Hook Reading</TableHead>
                  </TableRow>
                </TableHeader>
                <TableBody>
                  {[...diag.boot_lines].reverse().map((ls, i) => (
                    <TableRow key={i}>
                      <TableCell className="font-mono text-muted-foreground">{ls.boot}</TableCell>
                      <TableCell className="text-right font-mono">{ls.raw}</TableCell>
                    </TableRow>
                  ))}
                </TableBody>
              </Table>
            )}
          </CardContent>
        </Card>
      </div>

      <Card>
        <CardHeader className="pb-3 border-b border-border/50">
          <CardTitle className="text-lg flex items-center gap-2">
            <Cpu className="w-5 h-5 text-primary" />
            System Information
          </CardTitle>
        </CardHeader>
        <CardContent className="pt-6">
          <div className="grid grid-cols-2 gap-4 text-sm mb-6">
            <div>
              <span className="text-muted-foreground block mb-1">Firmware Version</span>
              <span className="font-mono bg-muted px-2 py-1 rounded">{sysInfo.firmware ?? '—'}</span>
            </div>
            <div>
              <span className="text-muted-foreground block mb-1">Uptime</span>
              <span className="font-mono">{uH}h {uM}m</span>
            </div>
            <div>
              <span className="text-muted-foreground block mb-1">Memory Free</span>
              <span className="font-mono">{heapKB} KB</span>
            </div>
            <div>
              <span className="text-muted-foreground block mb-1">SD Card</span>
              <span className={sysInfo.sd ? "text-emerald-400 font-mono" : "text-destructive font-mono"}>
                {sysInfo.sd ? `OK${sysInfo.sd_total ? ` (${sysInfo.sd_used}MB / ${sysInfo.sd_total}MB)` : ''}` : 'Not detected'}
              </span>
            </div>
            {sysInfo.wifi_mode && (
              <>
                <div>
                  <span className="text-muted-foreground block mb-1 flex items-center gap-1"><Wifi className="w-3 h-3" /> Wi-Fi Mode</span>
                  <span className="font-mono">{sysInfo.wifi_mode === 'ap' ? 'Hotspot' : 'Station'}</span>
                </div>
                <div>
                  <span className="text-muted-foreground block mb-1">Network / IP</span>
                  <span className="font-mono">{sysInfo.wifi_ssid} · {sysInfo.wifi_ip}</span>
                </div>
              </>
            )}
          </div>
          <Button variant="destructive" className="w-full" onClick={reboot}>
            <RefreshCw className="w-4 h-4 mr-2" />
            Restart Telephone
          </Button>
        </CardContent>
      </Card>
    </div>
  )
}
