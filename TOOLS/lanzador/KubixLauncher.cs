// KUBIX.exe: app de escritorio para abrir los juegos con un clic.
// Se compila con TOOLS\compilar_windows.bat (csc.exe de .NET Framework, sin instalar nada).
//
// Uso:
//   KUBIX.exe                       abre la ventana
//   KUBIX.exe --lanzar pong [seg]   abre un juego directo (clave: roadfighter, spaceinvaders,
//                                   snake, pong, tetris, breakout, menu); con seg lo cierra
//                                   a los seg segundos. Codigos: 0 bien, 2 sin compilar,
//                                   3 el juego fallo, 4 clave desconocida.
//   KUBIX.exe --captura salida.png  guarda una imagen de la ventana (para comprobar el diseno)
using System;
using System.Diagnostics;
using System.Drawing;
using System.Drawing.Drawing2D;
using System.IO;
using System.Windows.Forms;

namespace Kubix
{
    class Game
    {
        public string Key, Name, Folder, Exe, Info, Controls;
        public Game(string key, string name, string folder, string exe, string info, string controls)
        {
            Key = key; Name = name; Folder = folder; Exe = exe; Info = info; Controls = controls;
        }
    }

    static class Theme
    {
        public static readonly Color Bg = Color.FromArgb(12, 14, 19);
        public static readonly Color Panel = Color.FromArgb(20, 22, 30);
        public static readonly Color PanelHover = Color.FromArgb(28, 32, 46);
        public static readonly Color Border = Color.FromArgb(51, 61, 96);
        public static readonly Color Accent = Color.FromArgb(85, 177, 255);
        public static readonly Color Text = Color.FromArgb(236, 240, 248);
        public static readonly Color Muted = Color.FromArgb(150, 160, 184);
        public static readonly Color Ok = Color.FromArgb(91, 227, 125);
        public static readonly Color Warn = Color.FromArgb(255, 180, 84);
    }

    static class Locator
    {
        public static readonly Game[] Games = new Game[]
        {
            new Game("roadfighter", "Road Fighter", "roadFighter", "roadFighter", "Carreras  ·  1 jugador",
                     "↑ acelera  ·  ↓ frena  ·  ← → carril  ·  Enter pausa"),
            new Game("spaceinvaders", "Space Invaders", "space_invaders", "space_invaders", "Disparos  ·  1 o 2 jugadores",
                     "← → mover  ·  A disparar  ·  Jugador 2: J L y K"),
            new Game("snake", "Snake", "snake", "snake", "La serpiente  ·  1 o 2 jugadores",
                     "Jugador 1: flechas  ·  Jugador 2: W A S D  ·  Enter pausa"),
            new Game("pong", "Pong", "pong", "pong", "Paletas  ·  contra la CPU o 2 jugadores",
                     "↑ ↓ paleta  ·  Jugador 2: W / S  ·  Enter inicia o pausa"),
            new Game("tetris", "Tetris", "Tetris", "tetris", "Rompecabezas  ·  1 jugador",
                     "← → mover  ·  ↓ cae  ·  ↑ cae directo  ·  A / B rotan"),
            new Game("breakout", "Breakout", "breakout", "breakout", "Rompe bloques  ·  1 o 2 jugadores",
                     "← → paleta  ·  A lanza la pelota  ·  Jugador 2: J L y K")
        };
        public static readonly Game Menu = new Game("menu", "Menú KUBIX", "menu", "kubix", "Menú de la consola",
                     "Flechas mueven  ·  A abre el juego  ·  B vuelve");

        public static string FindRoot()
        {
            string dir = AppDomain.CurrentDomain.BaseDirectory.TrimEnd('\\', '/');
            for (int i = 0; i < 5 && dir != null; i++)
            {
                if (Directory.Exists(Path.Combine(dir, "menu")) && Directory.Exists(Path.Combine(dir, "pong")))
                    return dir;
                DirectoryInfo up = Directory.GetParent(dir);
                dir = up == null ? null : up.FullName;
            }
            return null;
        }

        public static string FindExe(string root, Game g)
        {
            string[] subs = new string[] { "build", "build\\Release", "build\\Debug" };
            foreach (string sub in subs)
            {
                string p = Path.Combine(root, g.Folder + "\\" + sub + "\\" + g.Exe + ".exe");
                if (File.Exists(p)) return p;
            }
            return null;
        }

        public static Game ByKey(string key)
        {
            if (key == Menu.Key) return Menu;
            foreach (Game g in Games) if (g.Key == key) return g;
            return null;
        }

        public static ProcessStartInfo StartInfo(string root, Game g, string exe)
        {
            ProcessStartInfo psi = new ProcessStartInfo(exe);
            psi.WorkingDirectory = Path.GetDirectoryName(exe);
            psi.UseShellExecute = false;
            psi.CreateNoWindow = true;
            if (g == Menu) psi.Arguments = "\"" + root.Replace('\\', '/') + "\"";
            return psi;
        }
    }

    class Tile : Control
    {
        public Game Game;
        public Image Picture;
        public bool Selected, Hover, Running;
        public string Status = "";
        public Color StatusColor = Theme.Muted;
        static readonly Font NameFont = new Font("Segoe UI", 14f, FontStyle.Bold);
        static readonly Font InfoFont = new Font("Segoe UI", 9.5f);
        static readonly Font SmallFont = new Font("Segoe UI", 8.5f);

        public Tile(Game g, Image picture)
        {
            Game = g; Picture = picture;
            SetStyle(ControlStyles.UserPaint | ControlStyles.AllPaintingInWmPaint | ControlStyles.OptimizedDoubleBuffer |
                     ControlStyles.ResizeRedraw | ControlStyles.Selectable, true);
            Cursor = Cursors.Hand;
            MouseEnter += delegate { Hover = true; Invalidate(); };
            MouseLeave += delegate { Hover = false; Invalidate(); };
        }

        public static GraphicsPath Round(Rectangle r, int radius)
        {
            int d = radius * 2;
            GraphicsPath p = new GraphicsPath();
            p.AddArc(r.X, r.Y, d, d, 180, 90);
            p.AddArc(r.Right - d, r.Y, d, d, 270, 90);
            p.AddArc(r.Right - d, r.Bottom - d, d, d, 0, 90);
            p.AddArc(r.X, r.Bottom - d, d, d, 90, 90);
            p.CloseFigure();
            return p;
        }

        protected override void OnPaint(PaintEventArgs e)
        {
            Graphics g = e.Graphics;
            g.Clear(Theme.Bg);
            g.SmoothingMode = SmoothingMode.AntiAlias;
            Rectangle r = new Rectangle(1, 1, Width - 3, Height - 3);
            using (GraphicsPath path = Round(r, 14))
            using (SolidBrush fill = new SolidBrush(Hover && !Running ? Theme.PanelHover : Theme.Panel))
            using (Pen border = new Pen(Selected ? Theme.Accent : Theme.Border, Selected ? 2.5f : 1.5f))
            {
                g.FillPath(fill, path);
                g.DrawPath(border, path);
            }

            int size = 124;
            Rectangle ir = new Rectangle((Width - size) / 2, 12, size, size);
            using (Pen frame = new Pen(Theme.Border, 1f))
            {
                if (Picture != null)
                {
                    g.InterpolationMode = InterpolationMode.NearestNeighbor;
                    g.PixelOffsetMode = PixelOffsetMode.Half;
                    g.DrawImage(Picture, ir);
                    g.PixelOffsetMode = PixelOffsetMode.Default;
                }
                else
                {
                    using (SolidBrush b = new SolidBrush(Color.Black)) g.FillRectangle(b, ir);
                }
                g.DrawRectangle(frame, ir);
            }

            TextFormatFlags center = TextFormatFlags.HorizontalCenter | TextFormatFlags.VerticalCenter |
                                     TextFormatFlags.SingleLine | TextFormatFlags.NoPadding | TextFormatFlags.EndEllipsis;
            TextRenderer.DrawText(g, Game.Name, NameFont, new Rectangle(8, 140, Width - 16, 24), Theme.Text, center);
            TextRenderer.DrawText(g, Game.Info, InfoFont, new Rectangle(8, 165, Width - 16, 18), Theme.Muted, center);
            TextRenderer.DrawText(g, Status, InfoFont, new Rectangle(8, 185, Width - 16, 18), StatusColor, center);
            TextRenderer.DrawText(g, Game.Controls, SmallFont, new Rectangle(14, 206, Width - 28, 28), Theme.Muted,
                                  TextFormatFlags.HorizontalCenter | TextFormatFlags.WordBreak | TextFormatFlags.NoPadding);
        }
    }

    class MainForm : Form
    {
        readonly string root;
        readonly Tile[] tiles;
        Label summary;
        int selected;
        Process running;
        Tile runningTile;

        const int TileW = 320, TileH = 240, Gap = 12, Margin2 = 16, Top = 76;

        public MainForm(string root)
        {
            this.root = root;
            Text = "KUBIX · Software de juegos";
            BackColor = Theme.Bg;
            ForeColor = Theme.Text;
            Font = new Font("Segoe UI", 9f);
            FormBorderStyle = FormBorderStyle.FixedSingle;
            MaximizeBox = false;
            StartPosition = FormStartPosition.CenterScreen;
            ClientSize = new Size(3 * TileW + 2 * Gap + 2 * Margin2, Top + 2 * TileH + Gap + 10 + 96);
            KeyPreview = true;
            try { Icon = Icon.ExtractAssociatedIcon(Application.ExecutablePath); } catch (Exception) { }

            Label title = new Label();
            title.Text = "KUBIX";
            title.Font = new Font("Segoe UI", 26f, FontStyle.Bold);
            title.ForeColor = Theme.Accent;
            title.AutoSize = true;
            title.Location = new Point(Margin2, 10);
            Controls.Add(title);

            Label sub = new Label();
            sub.Text = "Software de juegos  ·  haz clic en un juego para jugarlo";
            sub.Font = new Font("Segoe UI", 11f);
            sub.ForeColor = Theme.Muted;
            sub.AutoSize = true;
            sub.Location = new Point(Margin2 + 150, 28);
            Controls.Add(sub);

            summary = new Label();
            summary.Font = new Font("Segoe UI", 10f, FontStyle.Bold);
            summary.AutoSize = false;
            summary.TextAlign = ContentAlignment.MiddleRight;
            summary.Size = new Size(260, 24);
            summary.Location = new Point(ClientSize.Width - Margin2 - 260, 30);
            Controls.Add(summary);

            tiles = new Tile[Locator.Games.Length];
            for (int i = 0; i < tiles.Length; i++)
            {
                Game game = Locator.Games[i];
                Tile t = new Tile(game, LoadPicture(game));
                t.Size = new Size(TileW, TileH);
                t.Location = new Point(Margin2 + (i % 3) * (TileW + Gap), Top + (i / 3) * (TileH + Gap));
                int index = i;
                t.Click += delegate { SelectTile(index); Launch(Locator.Games[index]); };
                Controls.Add(t);
                tiles[i] = t;
            }

            int by = Top + 2 * TileH + Gap + 10;
            Label help = new Label();
            help.Text = "Teclado = control NES:  Flechas = mover  ·  Enter = Start  ·  tecla A = botón A  ·  tecla B = botón B.\r\n" +
                        "Para salir de un juego usa su opción Exit o cierra su ventana: esta pantalla vuelve sola.";
            help.ForeColor = Theme.Muted;
            help.AutoSize = false;
            help.Size = new Size(ClientSize.Width - 2 * Margin2, 40);
            help.Location = new Point(Margin2, by + 2);
            Controls.Add(help);

            int bx = ClientSize.Width - Margin2;
            bx = AddButton("Abrir carpeta", bx, by + 44, OpenFolder);
            bx = AddButton("Compilar / actualizar", bx, by + 44, Compile);
            AddButton("Menú KUBIX (consola)", bx, by + 44, delegate { Launch(Locator.Menu); });

            SelectTile(0);
            RefreshStatus();
        }

        int AddButton(string text, int right, int y, EventHandler onClick)
        {
            Button b = new Button();
            b.Text = text;
            b.FlatStyle = FlatStyle.Flat;
            b.FlatAppearance.BorderColor = Theme.Border;
            b.FlatAppearance.MouseOverBackColor = Theme.PanelHover;
            b.BackColor = Theme.Panel;
            b.ForeColor = Theme.Text;
            b.Size = new Size(text.Length * 8 + 28, 34);
            b.Location = new Point(right - b.Width, y);
            b.Cursor = Cursors.Hand;
            b.TabStop = false;
            b.Click += onClick;
            Controls.Add(b);
            return right - b.Width - 10;
        }

        Image LoadPicture(Game g)
        {
            foreach (string name in new string[] { "juego.png", "menu.png" })
            {
                string p = Path.Combine(root, g.Folder + "\\img\\" + name);
                if (!File.Exists(p)) continue;
                try
                {
                    using (FileStream fs = File.OpenRead(p))
                    using (Image src = Image.FromStream(fs))
                        return new Bitmap(src);
                }
                catch (Exception) { }
            }
            return null;
        }

        void SelectTile(int index)
        {
            selected = Math.Max(0, Math.Min(tiles.Length - 1, index));
            for (int i = 0; i < tiles.Length; i++) { tiles[i].Selected = i == selected; tiles[i].Invalidate(); }
        }

        public void RefreshStatus()
        {
            int ready = 0;
            foreach (Tile t in tiles)
            {
                bool ok = Locator.FindExe(root, t.Game) != null;
                if (ok) ready++;
                if (t == runningTile) { t.Status = "En ejecución…"; t.StatusColor = Theme.Accent; }
                else if (ok) { t.Status = "Listo para jugar"; t.StatusColor = Theme.Ok; }
                else { t.Status = "Sin compilar"; t.StatusColor = Theme.Warn; }
                t.Invalidate();
            }
            summary.Text = ready + " de " + tiles.Length + " juegos listos";
            summary.ForeColor = ready == tiles.Length ? Theme.Ok : Theme.Warn;
        }

        protected override bool ProcessCmdKey(ref Message msg, Keys keyData)
        {
            switch (keyData)
            {
                case Keys.Left: SelectTile(selected - 1); return true;
                case Keys.Right: SelectTile(selected + 1); return true;
                case Keys.Up: SelectTile(selected - 3); return true;
                case Keys.Down: SelectTile(selected + 3); return true;
                case Keys.Enter: Launch(Locator.Games[selected]); return true;
                case Keys.F5: RefreshStatus(); return true;
                case Keys.M: Launch(Locator.Menu); return true;
            }
            if (keyData >= Keys.D1 && keyData <= Keys.D6)
            {
                int i = (int)keyData - (int)Keys.D1;
                SelectTile(i);
                Launch(Locator.Games[i]);
                return true;
            }
            return base.ProcessCmdKey(ref msg, keyData);
        }

        void Launch(Game g)
        {
            if (running != null) return;
            string exe = Locator.FindExe(root, g);
            if (exe == null)
            {
                DialogResult r = MessageBox.Show(this,
                    g.Name + " todavía no está compilado.\r\n\r\n¿Compilar ahora todos los juegos? (tarda unos segundos)",
                    "KUBIX", MessageBoxButtons.YesNo, MessageBoxIcon.Question);
                if (r == DialogResult.Yes) Compile(null, EventArgs.Empty);
                return;
            }
            try
            {
                Process p = Process.Start(Locator.StartInfo(root, g, exe));
                running = p;
                runningTile = null;
                foreach (Tile t in tiles) if (t.Game == g) runningTile = t;
                p.EnableRaisingEvents = true;
                p.Exited += delegate
                {
                    try { BeginInvoke((Action)Back); } catch (Exception) { }
                };
                RefreshStatus();
                WindowState = FormWindowState.Minimized;
            }
            catch (Exception ex)
            {
                running = null;
                MessageBox.Show(this, "No se pudo abrir " + g.Name + ":\r\n" + ex.Message, "KUBIX", MessageBoxButtons.OK, MessageBoxIcon.Error);
            }
        }

        void Back()
        {
            running = null;
            runningTile = null;
            RefreshStatus();
            WindowState = FormWindowState.Normal;
            Activate();
        }

        void Compile(object sender, EventArgs e)
        {
            string script = Path.Combine(root, "TOOLS\\compilar_windows.ps1");
            if (!File.Exists(script))
            {
                MessageBox.Show(this, "No se encontró TOOLS\\compilar_windows.ps1", "KUBIX", MessageBoxButtons.OK, MessageBoxIcon.Warning);
                return;
            }
            try
            {
                ProcessStartInfo psi = new ProcessStartInfo("powershell.exe",
                    "-NoProfile -ExecutionPolicy Bypass -File \"" + script + "\" -SinLanzador");
                psi.WorkingDirectory = root;
                psi.UseShellExecute = true;
                Process p = Process.Start(psi);
                p.EnableRaisingEvents = true;
                p.Exited += delegate { try { BeginInvoke((Action)RefreshStatus); } catch (Exception) { } };
            }
            catch (Exception ex)
            {
                MessageBox.Show(this, ex.Message, "KUBIX", MessageBoxButtons.OK, MessageBoxIcon.Error);
            }
        }

        void OpenFolder(object sender, EventArgs e)
        {
            Process.Start("explorer.exe", "\"" + root + "\"");
        }

        public void SaveSnapshot(string path)
        {
            Opacity = 0;                // se "muestra" sin que se vea, solo para poder fotografiarla
            ShowInTaskbar = false;
            Show();
            Application.DoEvents();
            Bitmap bmp = new Bitmap(Width, Height);
            DrawToBitmap(bmp, new Rectangle(0, 0, Width, Height));
            bmp.Save(path, System.Drawing.Imaging.ImageFormat.Png);
            Close();
        }
    }

    static class Program
    {
        [STAThread]
        static int Main(string[] args)
        {
            string root = Locator.FindRoot();
            if (root == null)
            {
                MessageBox.Show("KUBIX.exe debe estar en la carpeta del proyecto (la que contiene las carpetas menu y pong).",
                                "KUBIX", MessageBoxButtons.OK, MessageBoxIcon.Warning);
                return 1;
            }

            if (args.Length >= 2 && args[0] == "--lanzar")
                return LaunchDirect(root, args[1].ToLowerInvariant(), args.Length >= 3 ? int.Parse(args[2]) : 0);

            Application.EnableVisualStyles();
            Application.SetCompatibleTextRenderingDefault(false);
            MainForm form = new MainForm(root);
            if (args.Length >= 2 && args[0] == "--captura")
            {
                form.SaveSnapshot(args[1]);
                return 0;
            }
            Application.Run(form);
            return 0;
        }

        static int LaunchDirect(string root, string key, int seconds)
        {
            Game g = Locator.ByKey(key);
            if (g == null) return 4;
            string exe = Locator.FindExe(root, g);
            if (exe == null) return 2;
            using (Process p = Process.Start(Locator.StartInfo(root, g, exe)))
            {
                if (seconds <= 0) { p.WaitForExit(); return p.ExitCode == 0 ? 0 : 3; }
                if (p.WaitForExit(seconds * 1000)) return p.ExitCode == 0 ? 0 : 3;
                try { p.Kill(); } catch (Exception) { }
                return 0;
            }
        }
    }
}
