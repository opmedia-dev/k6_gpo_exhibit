import { useEffect, useState } from "react"
import { Card, CardContent, CardHeader, CardTitle, CardDescription } from "@/components/ui/card"
import { Button } from "@/components/ui/button"
import { Input } from "@/components/ui/input"
import { Table, TableBody, TableCell, TableHead, TableHeader, TableRow } from "@/components/ui/table"
import { BarChart2, Hash, BookOpen, Trash2, Plus } from "lucide-react"

interface StatsData {
  pickups?: number
  incoming?: number
  answered?: number
  outgoing?: number
  not_recognised?: number
  coin_collected?: number
  coin_refunded?: number
  call_count?: number
  avg_call?: number
  longest_call?: number
  call_seconds?: number
  completions?: number
  first_digit_n?: number
  first_digit_ms?: number
  total_uptime?: number
  top_numbers?: { number: string; count: number }[]
}

interface Discovery { number: string; count: number }
interface Alias { number: string; name: string }

export function StatsTab() {
  const [stats, setStats] = useState<StatsData>({})
  const [numbersTried, setNumbersTried] = useState<Discovery[]>([])
  const [directory, setDirectory] = useState<Alias[]>([])
  const [newNum, setNewNum] = useState("")
  const [newFile, setNewFile] = useState("")
  const [aliasStatus, setAliasStatus] = useState("")

  const loadAll = () => {
    fetch('/api/stats').then(r => r.json()).then(setStats).catch(() => {})
    fetch('/api/discovery').then(r => r.json()).then(d => setNumbersTried(d || [])).catch(() => {})
    fetch('/api/aliases').then(r => r.json()).then(d => setDirectory(d || [])).catch(() => {})
  }

  useEffect(() => { loadAll() }, [])

  const clearNumbers = () => {
    if (!confirm('Clear the numbers tried log?')) return
    fetch('/api/discovery/clear', { method: 'POST' }).then(() => {
      fetch('/api/discovery').then(r => r.json()).then(d => setNumbersTried(d || [])).catch(() => {})
    }).catch(() => {})
  }

  const removeDiscovery = (num: string) => {
    fetch('/api/discovery/remove?number=' + encodeURIComponent(num), { method: 'POST' })
      .then(() => setNumbersTried(prev => prev.filter(d => d.number !== num)))
      .catch(() => {})
  }

  const addAlias = () => {
    if (!newNum || !newFile) return
    const updated = [...directory]
    const idx = updated.findIndex(a => a.number === newNum)
    if (idx >= 0) updated[idx].name = newFile
    else updated.push({ number: newNum, name: newFile })
    fetch('/api/aliases', {
      method: 'POST',
      headers: { 'Content-Type': 'application/json' },
      body: JSON.stringify(updated),
    }).then(r => r.json()).then(d => {
      setAliasStatus(d.ok ? 'Saved' : (d.error ?? 'Error'))
      setDirectory(updated)
      setNewNum(''); setNewFile('')
    }).catch(() => { setAliasStatus('Error') })
  }

  const removeAlias = (num: string) => {
    const updated = directory.filter(a => a.number !== num)
    fetch('/api/aliases', {
      method: 'POST',
      headers: { 'Content-Type': 'application/json' },
      body: JSON.stringify(updated),
    }).then(r => r.json()).then(d => {
      if (d.ok) setDirectory(updated)
    }).catch(() => {})
  }

  const fmt = (secs: number) => `${Math.floor(secs / 60)}m ${secs % 60}s`
  const uH = Math.floor((stats.total_uptime ?? 0) / 3600)
  const uM = Math.floor(((stats.total_uptime ?? 0) % 3600) / 60)
  const compRate = (stats.pickups ?? 0) > 0 ? Math.min(100, Math.round(((stats.completions ?? 0) / stats.pickups!) * 100)) : 0
  const avgFd = (stats.first_digit_n ?? 0) > 0 ? Math.round((stats.first_digit_ms! / stats.first_digit_n!) / 100) / 10 : 0

  return (
    <div className="space-y-6">
      <Card>
        <CardHeader className="pb-3 border-b border-border/50">
          <CardTitle className="text-lg flex items-center gap-2">
            <BarChart2 className="w-5 h-5 text-primary" />
            Visitor Activity (All Time)
          </CardTitle>
        </CardHeader>
        <CardContent className="pt-6">
          <div className="grid grid-cols-2 sm:grid-cols-3 gap-6">
            {[
              { label: 'Sessions (Pickups)', value: stats.pickups ?? 0 },
              { label: 'Times Rung', value: stats.incoming ?? 0 },
              { label: 'Calls Answered', value: stats.answered ?? 0 },
              { label: 'Numbers Dialled', value: (stats.outgoing ?? 0).toLocaleString() },
              { label: 'Unknown Numbers', value: stats.not_recognised ?? 0 },
              { label: 'Completion Rate', value: `${compRate}%` },
              ...(stats.call_count && stats.call_count > 0 ? [
                { label: 'Average Call', value: fmt(stats.avg_call ?? 0) },
                { label: 'Longest Call', value: fmt(stats.longest_call ?? 0) },
                { label: 'Total Talk Time', value: `${Math.floor((stats.call_seconds ?? 0) / 60)} min` },
              ] : []),
              ...(avgFd > 0 ? [{ label: 'Avg. Time to First Digit', value: `${avgFd}s` }] : []),
            ].map((item, i) => (
              <div key={i}>
                <p className="stat-label">{item.label}</p>
                <p className="stat-value">{item.value}</p>
              </div>
            ))}
            <div className="col-span-2 sm:col-span-3 border-t border-border/50 pt-4 mt-2">
              <p className="stat-label">Total Running Time</p>
              <p className="stat-value text-accent">{uH}h {uM}m</p>
            </div>
          </div>
          {stats.top_numbers && stats.top_numbers.length > 0 && (
            <div className="mt-4 pt-4 border-t border-border/50">
              <p className="text-sm font-medium text-muted-foreground mb-2">Most Popular Numbers</p>
              <div className="flex flex-wrap gap-2">
                {stats.top_numbers.map(n => (
                  <span key={n.number} className="bg-muted px-3 py-1 rounded font-mono text-sm">
                    {n.number} <span className="text-muted-foreground">×{n.count}</span>
                  </span>
                ))}
              </div>
            </div>
          )}
          <div className="mt-4 pt-4 border-t border-border/50">
            <Button variant="destructive" size="sm" onClick={() => {
              if (!confirm('This will erase all visitor statistics. Are you sure?')) return
              fetch('/api/stats/reset', { method: 'POST' }).then(loadAll).catch(() => {})
            }}>Clear All Statistics</Button>
          </div>
        </CardContent>
      </Card>

      <Card>
        <CardHeader className="pb-3 border-b border-border/50">
          <div className="flex flex-wrap items-start justify-between gap-3">
            <div className="space-y-1 min-w-0">
              <CardTitle className="text-lg flex items-center gap-2">
                <Hash className="w-5 h-5 text-primary shrink-0" />
                Numbers Tried (Unknown)
              </CardTitle>
              <CardDescription>Numbers dialled that are not in the directory</CardDescription>
            </div>
            <Button variant="destructive" size="sm" onClick={clearNumbers} disabled={numbersTried.length === 0} className="shrink-0">
              Clear All
            </Button>
          </div>
        </CardHeader>
        <CardContent className="p-0">
          {numbersTried.length === 0 ? (
            <div className="p-6 text-center text-muted-foreground text-sm">No unknown numbers recorded.</div>
          ) : (
            <Table>
              <TableHeader>
                <TableRow>
                  <TableHead>Number</TableHead>
                  <TableHead className="text-right">Times Tried</TableHead>
                  <TableHead className="w-[50px]"></TableHead>
                </TableRow>
              </TableHeader>
              <TableBody>
                {numbersTried.map((item, i) => (
                  <TableRow key={i}>
                    <TableCell className="font-mono">{item.number}</TableCell>
                    <TableCell className="text-right">{item.count}</TableCell>
                    <TableCell>
                      <Button variant="ghost" size="icon" onClick={() => removeDiscovery(item.number)} className="h-8 w-8 text-muted-foreground hover:text-destructive">
                        <Trash2 className="h-4 w-4" />
                      </Button>
                    </TableCell>
                  </TableRow>
                ))}
              </TableBody>
            </Table>
          )}
        </CardContent>
      </Card>

      <Card>
        <CardHeader className="pb-3 border-b border-border/50">
          <CardTitle className="text-lg flex items-center gap-2">
            <BookOpen className="w-5 h-5 text-primary" />
            Number Directory
          </CardTitle>
          <CardDescription>Map dialled numbers to audio files in /numbers/</CardDescription>
        </CardHeader>
        <CardContent className="p-0">
          <Table>
            <TableHeader>
              <TableRow>
                <TableHead>Dial Number</TableHead>
                <TableHead>Audio File</TableHead>
                <TableHead className="w-[50px]"></TableHead>
              </TableRow>
            </TableHeader>
            <TableBody>
              {directory.map((item) => (
                <TableRow key={item.number}>
                  <TableCell className="font-mono">{item.number}</TableCell>
                  <TableCell className="text-muted-foreground font-mono text-sm">{item.name}</TableCell>
                  <TableCell>
                    <Button variant="ghost" size="icon" onClick={() => removeAlias(item.number)} className="h-8 w-8 text-muted-foreground hover:text-destructive">
                      <Trash2 className="h-4 w-4" />
                    </Button>
                  </TableCell>
                </TableRow>
              ))}
            </TableBody>
          </Table>
          <div className="p-4 bg-muted/30 border-t border-border/50 flex gap-3 items-end flex-wrap">
            <div className="space-y-1 flex-1 min-w-[80px]">
              <label className="text-xs font-medium text-muted-foreground">Dial Number</label>
              <Input placeholder="e.g. 999" value={newNum} onChange={e => setNewNum(e.target.value)} />
            </div>
            <div className="space-y-1 flex-1 min-w-[120px]">
              <label className="text-xs font-medium text-muted-foreground">Audio File (in /numbers/)</label>
              <Input placeholder="e.g. emergency" value={newFile} onChange={e => setNewFile(e.target.value)} />
            </div>
            <Button onClick={addAlias} disabled={!newNum || !newFile}>
              <Plus className="h-4 w-4 mr-1" /> Add
            </Button>
          </div>
          {aliasStatus && <p className="px-4 pb-3 text-sm text-muted-foreground">{aliasStatus}</p>}
        </CardContent>
      </Card>
    </div>
  )
}
