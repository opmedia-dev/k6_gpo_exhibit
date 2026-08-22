import { useState } from "react"
import { Tabs, TabsContent, TabsList, TabsTrigger } from "@/components/ui/tabs"
import { Button } from "@/components/ui/button"
import { Phone, BarChart2, Stethoscope, Terminal, Settings, Moon, Sun } from "lucide-react"
import { useTheme } from "@/hooks/use-theme"
import { OverviewTab } from "@/components/k6/OverviewTab"
import { StatsTab } from "@/components/k6/StatsTab"
import { DiagnosticsTab } from "@/components/k6/DiagnosticsTab"
import { TerminalTab } from "@/components/k6/TerminalTab"
import { SettingsTab } from "@/components/k6/SettingsTab"

function ThemeToggle() {
  const { theme, setTheme } = useTheme()

  return (
    <Button
      variant="ghost"
      size="icon"
      onClick={() => setTheme(theme === "light" ? "dark" : "light")}
      className="text-muted-foreground hover:text-foreground"
    >
      {theme === "light" ? <Moon className="h-5 w-5" /> : <Sun className="h-5 w-5" />}
    </Button>
  )
}

export function AppShell() {
  const [activeTab, setActiveTab] = useState("overview")

  return (
    <div className="min-h-[100dvh] bg-background text-foreground flex flex-col font-sans selection:bg-primary/30">
      
      {/* Header */}
      <header className="sticky top-0 z-50 w-full border-b border-border/60 bg-background/95 backdrop-blur supports-[backdrop-filter]:bg-background/80">
        <div className="mx-auto w-full max-w-[680px] flex h-14 items-center justify-between px-4">
          <div className="flex items-center gap-2">
            <div className="bg-primary text-primary-foreground p-1.5 rounded-md shadow-sm">
              <Phone className="w-5 h-5" />
            </div>
            <div className="flex flex-col leading-none">
              <span className="font-semibold text-sm tracking-tight">GPO Phone Emulator</span>
              <span className="text-[10px] text-muted-foreground uppercase tracking-widest font-medium">Control Panel</span>
            </div>
          </div>
          <ThemeToggle />
        </div>
      </header>

      {/* Main Content */}
      <main className="flex-1 w-full max-w-[680px] mx-auto p-4 pb-20">
        <Tabs value={activeTab} onValueChange={setActiveTab} className="w-full">
          <div className="sticky top-14 z-40 bg-background/95 backdrop-blur pt-2 pb-4 -mx-4 px-4 sm:mx-0 sm:px-0">
            <TabsList className="w-full h-auto p-1 grid grid-cols-5 gap-1 bg-muted/50 rounded-xl">
              <TabsTrigger value="overview" className="flex flex-col gap-1 py-2 px-1 text-xs data-[state=active]:bg-primary data-[state=active]:text-primary-foreground data-[state=active]:shadow-md rounded-lg transition-all">
                <Phone className="w-4 h-4" />
                <span className="hidden sm:inline">Overview</span>
              </TabsTrigger>
              <TabsTrigger value="stats" className="flex flex-col gap-1 py-2 px-1 text-xs data-[state=active]:bg-primary data-[state=active]:text-primary-foreground data-[state=active]:shadow-md rounded-lg transition-all">
                <BarChart2 className="w-4 h-4" />
                <span className="hidden sm:inline">Stats</span>
              </TabsTrigger>
              <TabsTrigger value="diagnostics" className="flex flex-col gap-1 py-2 px-1 text-xs data-[state=active]:bg-primary data-[state=active]:text-primary-foreground data-[state=active]:shadow-md rounded-lg transition-all">
                <Stethoscope className="w-4 h-4" />
                <span className="hidden sm:inline">Diagnostics</span>
              </TabsTrigger>
              <TabsTrigger value="terminal" className="flex flex-col gap-1 py-2 px-1 text-xs data-[state=active]:bg-primary data-[state=active]:text-primary-foreground data-[state=active]:shadow-md rounded-lg transition-all">
                <Terminal className="w-4 h-4" />
                <span className="hidden sm:inline">Terminal</span>
              </TabsTrigger>
              <TabsTrigger value="settings" className="flex flex-col gap-1 py-2 px-1 text-xs data-[state=active]:bg-primary data-[state=active]:text-primary-foreground data-[state=active]:shadow-md rounded-lg transition-all">
                <Settings className="w-4 h-4" />
                <span className="hidden sm:inline">Settings</span>
              </TabsTrigger>
            </TabsList>
          </div>

          <div className="mt-2 animate-in fade-in slide-in-from-bottom-2 duration-300">
            <TabsContent value="overview" className="mt-0 outline-none">
              <OverviewTab />
            </TabsContent>
            
            <TabsContent value="stats" className="mt-0 outline-none">
              <StatsTab />
            </TabsContent>
            
            <TabsContent value="diagnostics" className="mt-0 outline-none">
              <DiagnosticsTab />
            </TabsContent>
            
            <TabsContent value="terminal" className="mt-0 outline-none">
              <TerminalTab />
            </TabsContent>
            
            <TabsContent value="settings" className="mt-0 outline-none">
              <SettingsTab />
            </TabsContent>
          </div>
        </Tabs>
      </main>
    </div>
  )
}
