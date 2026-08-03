import { useEffect, useState } from "react"
import { Card, CardContent, CardHeader, CardTitle } from "@/components/ui/card"
import { Button } from "@/components/ui/button"
import { Badge } from "@/components/ui/badge"
import { Phone, Clock, PlayCircle, Hash, Power, ShieldCheck, Activity, MemoryStick, Cpu, PhoneOff, AlertTriangle } from "lucide-react"

interface StatusData {
  mode: string
  state: string
  playing: string | null
  call_secs: number
  heap: number
  sd: boolean
  sd_total?: number
  sd_used?: number
  line_cal: boolean
  uptime: number
  errors?: number
  volume?: number
  bell_freq?: number
  digit_gap?: number
  line_level?: number
  ring_max?: number
  rt_min?: number
  rt_max?: number
  alert_idle?: number
  ar_min?: number
  ar_max?: number
  coin_override?: number
  coin_active?: boolean
  wifi_mode?: string
  wifi_ssid?: string
  wifi_ip?: string
  wifi_cfg_mode?: string
  wifi_cfg_ssid?: string
  alert_on?: boolean
  firmware?: string
}

interface SessionData {
  pickups?: number
  outgoing?: number
  incoming?: number
  completions?: number
  top_numbers?: { number: string; count: number }[]
  uptime?: number
}

export function OverviewTab() {
  const [status, setStatus] = useState<StatusData | null>(null)
  const [session, setSession] = useState<SessionData>({})
  const [callSecs, setCallSecs] = useState(0)

  const loadStatus = () => {
    fetch('/api/status')
      .then(r => r.json())
      .then((d: StatusData) => { setStatus(d) })
      .catch(() => {})
  }

  const loadSession = () => {
    fetch('/api/stats')
      .then(r => r.json())
      .then((d: { session?: SessionData }) => setSession(d.session || {}))
      .catch(() => {})
  }

  useEffect(() => {
    loadStatus()
    loadSession()
    const si = setInterval(loadStatus, 5000)
    const ss = setInterval(loadSession, 30000)
    return () => { clearInterval(si); clearInterval(ss) }
  }, [])

  // Local call timer driven by status
  useEffect(() => {
    if (!status) return
    if (status.state !== 'IDLE' && status.call_secs >= 0) {
      setCallSecs(status.call_secs)
      const t = setInterval(() => setCallSecs(s => s + 1), 1000)
      return () => clearInterval(t)
    } else {
      setCallSecs(0)
    }
  }, [status?.state])

  const ringNow = () => fetch('/api/ring', { method: 'POST' }).then(loadStatus).catch(() => {})
  const toggleMode = () => fetch('/api/mode', { method: 'POST' }).then(loadStatus).catch(() => {})

  const formatTime = (secs: number) => {
    const m = Math.floor(secs / 60)
    const s = secs % 60
    return `${m}:${s.toString().padStart(2, '0')}`
  }

  const stateLabels: Record<string, string> = {
    IDLE: 'Waiting for visitors',
    RINGING: 'Phone is ringing',
    PLAYING: 'Playing audio',
    DIALLING: 'Visitor is dialling',
  }

  const isManual = status?.mode !== 'AUTO'
  const isActive = status?.state !== 'IDLE' && status?.state !== undefined
  const heapKB = status ? Math.round(status.heap / 1024) : 0
  const sdOk = status?.sd ?? false
  const calOk = status?.line_cal ?? false
  const lowHeap = heapKB < 40
  const hasErrors = (status?.errors ?? 0) > 0
  const uH = status ? Math.floor(status.uptime / 3600) : 0
  const uM = status ? Math.floor((status.uptime % 3600) / 60) : 0

  const healthCls = !sdOk ? 'destructive' : (!calOk || lowHeap || hasErrors) ? 'warning' : 'success'
  const healthLabel = !sdOk ? 'SD FAULT' : (!calOk || lowHeap || hasErrors) ? 'CHECK' : 'HEALTHY'

  const pk = session.pickups ?? 0
  const completionRate = pk > 0 ? Math.round(((session.completions ?? 0) / pk) * 100) : 0
  const supH = Math.floor((session.uptime ?? 0) / 3600)
  const supM = Math.floor(((session.uptime ?? 0) % 3600) / 60)

  return (
    <div className="space-y-6">
      {status?.alert_on && (
        <div className="bg-destructive text-destructive-foreground px-4 py-3 rounded-lg font-medium text-sm">
          ⚠ No visitor activity detected for a while — please check the exhibit is working.
        </div>
      )}

      <Card>
        <CardHeader className="pb-3 border-b border-border/50">
          <div className="flex items-center justify-between">
            <CardTitle className="text-lg flex items-center gap-2">
              <Phone className="w-5 h-5 text-primary" />
              Phone Status
            </CardTitle>
            <Badge variant={isManual ? "warning" : "success"}>
              {isManual ? "MANUAL" : "AUTOMATIC"}
            </Badge>
          </div>
        </CardHeader>
        <CardContent className="pt-6 space-y-6">
          <div className="grid grid-cols-2 gap-4">
            <div className="space-y-1">
              <p className="text-sm text-muted-foreground">Current State</p>
              <p className="font-medium text-foreground flex items-center gap-2">
                {isActive ? (
                  <><span className="w-2 h-2 rounded-full bg-accent animate-pulse" />{stateLabels[status!.state] ?? status!.state}</>
                ) : (
                  <><span className="w-2 h-2 rounded-full bg-primary" /> Waiting for visitors</>
                )}
              </p>
            </div>
            <div className="space-y-1">
              <p className="text-sm text-muted-foreground">Now Playing</p>
              <p className="font-medium text-foreground flex items-center gap-2">
                {status?.playing ? (
                  <><PlayCircle className="w-4 h-4 text-muted-foreground" />{status.playing}</>
                ) : (
                  <span className="text-muted-foreground italic">Nothing</span>
                )}
              </p>
            </div>
            {isActive && callSecs >= 0 && (
              <div className="space-y-1 col-span-2">
                <p className="text-sm text-muted-foreground">Call Duration</p>
                <p className="font-mono text-xl">{formatTime(callSecs)}</p>
              </div>
            )}
          </div>

          <div className="flex items-center gap-3 pt-4 border-t border-border/50">
            <Button onClick={ringNow} variant="default" className="flex-1">
              <Phone className="w-4 h-4 mr-2" />
              Make Phone Ring
            </Button>
            <Button onClick={toggleMode} variant="outline" className="flex-1">
              {isManual ? "Switch to Auto" : "Switch to Manual"}
            </Button>
          </div>
        </CardContent>
      </Card>

      <div className="grid grid-cols-2 sm:grid-cols-3 gap-4">
        {[
          { icon: <Phone className="w-4 h-4 text-muted-foreground mb-2" />, value: session.pickups ?? 0, label: 'Pickups Today' },
          { icon: <Hash className="w-4 h-4 text-muted-foreground mb-2" />, value: session.outgoing ?? 0, label: 'Numbers Dialled' },
          { icon: <Activity className="w-4 h-4 text-muted-foreground mb-2" />, value: `${completionRate}%`, label: 'Completion Rate' },
          { icon: <AlertTriangle className="w-4 h-4 text-muted-foreground mb-2" />, value: session.incoming ?? 0, label: 'Times Rung' },
          { icon: <Hash className="w-4 h-4 text-muted-foreground mb-2" />, value: session.top_numbers?.length ? `${session.top_numbers[0].number} ×${session.top_numbers[0].count}` : '—', label: 'Most Dialled' },
          { icon: <Power className="w-4 h-4 text-muted-foreground mb-2" />, value: `${supH}h ${supM}m`, label: 'Powered On' },
        ].map((item, i) => (
          <Card key={i}>
            <CardContent className="p-4 space-y-1">
              {item.icon}
              <p className="text-2xl font-semibold">{item.value}</p>
              <p className="text-xs text-muted-foreground uppercase">{item.label}</p>
            </CardContent>
          </Card>
        ))}
      </div>

      <Card>
        <CardHeader className="pb-3 border-b border-border/50">
          <div className="flex items-center justify-between">
            <CardTitle className="text-lg flex items-center gap-2">
              <ShieldCheck className="w-5 h-5 text-primary" />
              System Health
            </CardTitle>
            <Badge variant={healthCls as 'success' | 'warning' | 'destructive'}>{healthLabel}</Badge>
          </div>
        </CardHeader>
        <CardContent className="pt-4">
          <div className="space-y-3 text-sm">
            <div className="flex justify-between items-center py-1">
              <span className="text-muted-foreground flex items-center gap-2"><MemoryStick className="w-4 h-4" /> SD Card</span>
              <span className={sdOk ? "text-foreground font-medium" : "text-destructive font-medium"}>
                {sdOk ? `Mounted${status?.sd_total ? ` (${(status.sd_total - (status.sd_used ?? 0))}MB free)` : ''}` : 'Not detected'}
              </span>
            </div>
            <div className="flex justify-between items-center py-1">
              <span className="text-muted-foreground flex items-center gap-2"><Activity className="w-4 h-4" /> Line Sensing</span>
              <span className={calOk ? "text-foreground font-medium" : "text-amber-500 font-medium"}>
                {calOk ? 'Calibrated' : 'Not calibrated'}
              </span>
            </div>
            <div className="flex justify-between items-center py-1">
              <span className="text-muted-foreground flex items-center gap-2"><Cpu className="w-4 h-4" /> Memory Free</span>
              <span className={lowHeap ? "text-amber-500 font-medium" : "text-foreground font-medium"}>{heapKB} KB</span>
            </div>
            <div className="flex justify-between items-center py-1">
              <span className="text-muted-foreground flex items-center gap-2"><Clock className="w-4 h-4" /> Uptime</span>
              <span className="font-medium text-foreground">{uH}h {uM}m</span>
            </div>
            {hasErrors && (
              <div className="flex justify-between items-center py-1">
                <span className="text-muted-foreground flex items-center gap-2"><AlertTriangle className="w-4 h-4" /> Errors</span>
                <span className="text-destructive font-medium">{status!.errors} recorded</span>
              </div>
            )}
          </div>
        </CardContent>
      </Card>
    </div>
  )
}
