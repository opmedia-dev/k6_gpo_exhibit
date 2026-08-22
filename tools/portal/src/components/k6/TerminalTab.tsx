import { useState, useRef, useEffect } from "react"
import { Card, CardContent, CardHeader, CardTitle } from "@/components/ui/card"
import { Button } from "@/components/ui/button"
import { Input } from "@/components/ui/input"
import { Terminal as TerminalIcon, Send, Cpu, Volume2, PhoneCall, PlayCircle } from "lucide-react"

export function TerminalTab() {
  const [output, setOutput] = useState([
    "K6 GPO Phone Emulator Console",
    "Type 'help' for a list of commands.",
    "ready >",
  ])
  const [cmd, setCmd] = useState("")
  const [history, setHistory] = useState<string[]>([])
  const [historyIdx, setHistoryIdx] = useState(-1)
  const [busy, setBusy] = useState(false)
  const outputRef = useRef<HTMLDivElement>(null)

  useEffect(() => {
    if (outputRef.current) outputRef.current.scrollTop = outputRef.current.scrollHeight
  }, [output])

  const print = (...lines: string[]) => setOutput(prev => [...prev, ...lines])

  const handleCommand = (c: string) => {
    const input = c.trim()
    if (!input || busy) return
    setHistory(prev => [...prev, input])
    setHistoryIdx(-1)
    setCmd("")
    print(`> ${input}`)
    setBusy(true)
    fetch('/api/terminal?cmd=' + encodeURIComponent(input), { method: 'POST' })
      .then(r => r.text())
      .then(t => { if (t) print(t); print("ready >") })
      .catch(e => print(`error: ${e}`, "ready >"))
      .finally(() => setBusy(false))
  }

  const handleKeyDown = (e: React.KeyboardEvent<HTMLInputElement>) => {
    if (e.key === "Enter") { handleCommand(cmd); return }
    if (e.key === "ArrowUp") {
      e.preventDefault()
      if (history.length > 0) {
        const idx = historyIdx === -1 ? history.length - 1 : Math.max(0, historyIdx - 1)
        setHistoryIdx(idx); setCmd(history[idx])
      }
    }
    if (e.key === "ArrowDown") {
      e.preventDefault()
      if (historyIdx !== -1) {
        const idx = historyIdx + 1
        if (idx >= history.length) { setHistoryIdx(-1); setCmd("") }
        else { setHistoryIdx(idx); setCmd(history[idx]) }
      }
    }
  }

  const quickCmd = (c: string) => handleCommand(c)

  return (
    <div className="space-y-6">
      <Card className="border-border">
        <CardHeader className="pb-3 border-b border-border/50 bg-card">
          <CardTitle className="text-lg flex items-center gap-2">
            <TerminalIcon className="w-5 h-5 text-primary" />
            Operator Console
          </CardTitle>
        </CardHeader>
        <CardContent className="p-0 flex flex-col h-[400px]">
          <div
            ref={outputRef}
            className="flex-1 overflow-y-auto p-4 bg-[#050505] font-mono text-sm text-[#10b981] whitespace-pre-wrap"
          >
            {output.join("\n")}
            {busy && <span className="animate-pulse">_</span>}
          </div>
          <div className="p-3 bg-card border-t border-border/50 flex gap-2">
            <div className="font-mono text-muted-foreground flex items-center pl-2">{">"}</div>
            <Input
              className="flex-1 bg-transparent border-0 shadow-none focus-visible:ring-0 font-mono text-foreground"
              value={cmd}
              onChange={e => setCmd(e.target.value)}
              onKeyDown={handleKeyDown}
              placeholder="Enter command…"
              autoComplete="off"
              autoCapitalize="off"
              spellCheck={false}
              autoFocus
              disabled={busy}
            />
            <Button size="icon" variant="ghost" onClick={() => handleCommand(cmd)} disabled={busy}>
              <Send className="w-4 h-4" />
            </Button>
          </div>
        </CardContent>
      </Card>

      <div className="space-y-4">
        <div>
          <p className="text-sm font-medium text-muted-foreground mb-3 uppercase tracking-wider">Quick Commands</p>
          <div className="flex flex-wrap gap-2">
            {['status','ring','hangup','ls /','sd','help'].map(c => (
              <Button key={c} variant="secondary" size="sm" onClick={() => quickCmd(c)} disabled={busy}>{c}</Button>
            ))}
            <Button variant="outline" size="sm" onClick={() => { setOutput(["ready >"]); setCmd("") }} disabled={busy}>clear</Button>
          </div>
        </div>

        <div>
          <p className="text-sm font-medium text-muted-foreground mb-3 uppercase tracking-wider">Commissioning Tools</p>
          <div className="grid grid-cols-2 sm:grid-cols-3 gap-2">
            <Button variant="outline" className="justify-start" onClick={() => quickCmd('selftest')} disabled={busy}>
              <Cpu className="w-4 h-4 mr-2" /> Self-Test
            </Button>
            <Button variant="outline" className="justify-start" onClick={() => quickCmd('probe')} disabled={busy}>
              <Volume2 className="w-4 h-4 mr-2" /> Audio Probe
            </Button>
            <Button variant="outline" className="justify-start" onClick={() => quickCmd('calibrate on')} disabled={busy}>
              <PhoneCall className="w-4 h-4 mr-2" /> Cal. On (hook down)
            </Button>
            <Button variant="outline" className="justify-start" onClick={() => quickCmd('calibrate off')} disabled={busy}>
              <PhoneCall className="w-4 h-4 mr-2" /> Cal. Off (hook up)
            </Button>
            <Button variant="outline" className="justify-start" onClick={() => quickCmd('dialecho')} disabled={busy}>
              <PlayCircle className="w-4 h-4 mr-2" /> Dial Echo Toggle
            </Button>
          </div>
        </div>
      </div>
    </div>
  )
}
