import { ThemeProvider } from '@/hooks/use-theme';
import { AppShell } from './AppShell';

function App() {
  return (
    <ThemeProvider defaultTheme="dark">
      <AppShell />
    </ThemeProvider>
  );
}

export default App;
