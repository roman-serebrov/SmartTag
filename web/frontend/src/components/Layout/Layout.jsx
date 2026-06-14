import { Outlet, NavLink } from 'react-router-dom';
import './Layout.css';

function Layout() {
  return (
    <div className="layout">
      <aside className="sidebar">
        <div className="logo">SmartTag</div>
        <nav>
          <NavLink to="/" end>Дашборд</NavLink>
          <NavLink to="/profiles">Профили</NavLink>
          <NavLink to="/ota">Обновление</NavLink>
          <NavLink to="/devices">Устройства</NavLink>
        </nav>
      </aside>
      <main className="content">
        <Outlet />
      </main>
    </div>
  );
}

export default Layout;