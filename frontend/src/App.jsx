import { useState, useEffect, useRef } from 'react'
import axios from 'axios'
import { Server, Users, Camera, KeyRound, Fingerprint, Trash2 } from 'lucide-react'

const API_BASE = 'http://localhost:8000/api'

function App() {
  const [users, setUsers] = useState([])
  const [status, setStatus] = useState({ pendingCommand: 'NONE', cameraIp: '' })
  const [enrollForm, setEnrollForm] = useState({ name: '', role: 'USER' })
  const [pinForm, setPinForm] = useState({ newPin: '', pinType: 'ADMIN' })
  
  const pendingRef = useRef('NONE')

  const fetchUsers = async () => {
    try {
      const res = await axios.get(`${API_BASE}/users`)
      setUsers(res.data)
    } catch (e) {
      console.error(e)
    }
  }

  const fetchStatus = async () => {
    try {
      const res = await axios.get(`${API_BASE}/system/status`)
      if (res.data.pendingCommand === 'NONE' && pendingRef.current !== 'NONE') {
        // A command just finished, auto reload the table!
        fetchUsers()
      }
      pendingRef.current = res.data.pendingCommand
      setStatus(res.data)
    } catch (e) {
      console.error(e)
    }
  }

  useEffect(() => {
    fetchUsers()
    fetchStatus()
    const interval = setInterval(fetchStatus, 1000)
    return () => clearInterval(interval)
  }, [])

  const handleEnroll = async (e) => {
    e.preventDefault()
    try {
      if (status.pendingCommand !== 'NONE') return alert('Hardware busy')
      await axios.post(`${API_BASE}/users/enroll`, enrollForm)
      setEnrollForm({ name: '', role: 'USER' })
      fetchStatus()
    } catch (e) {
      alert(e.response?.data?.detail || 'Error enrolling')
    }
  }

  const handleDelete = async (userId) => {
    try {
      if (status.pendingCommand !== 'NONE') return alert('Hardware busy')
      await axios.delete(`${API_BASE}/users/${userId}`)
      fetchStatus()
      fetchUsers()
    } catch (e) {
      alert(e.response?.data?.detail || 'Error deleting')
    }
  }

  const handlePinUpdate = async (e) => {
    e.preventDefault()
    try {
      if (status.pendingCommand !== 'NONE') return alert('Hardware busy')
      await axios.post(`${API_BASE}/settings/pin`, pinForm)
      setPinForm({ ...pinForm, newPin: '' })
      fetchStatus()
      alert(`${pinForm.pinType} PIN update command sent to lock`)
    } catch (e) {
      alert(e.response?.data?.detail || 'Error updating PIN')
    }
  }

  const handleClearCommand = async () => {
    try {
      if (!confirm('Are you sure you want to cancel the pending hardware command?')) return;
      await axios.post(`${API_BASE}/system/clear`);
      fetchStatus();
    } catch (e) {
      alert('Error clearing command');
    }
  }

  return (
    <div className="min-h-screen bg-slate-900 text-slate-100 p-8 font-sans w-full max-w-full m-0 text-left">
      <div className="max-w-6xl mx-auto space-y-8">
        
        {/* Header */}
        <header className="flex justify-between items-center bg-slate-800 p-6 rounded-2xl shadow-xl border border-slate-700">
          <div>
            <h1 className="text-3xl font-bold bg-clip-text text-transparent bg-gradient-to-r from-blue-400 to-indigo-400">
              ProxiGate Admin
            </h1>
            <p className="text-slate-400 mt-1">Smart Fingerprint Lock System</p>
          </div>
          <div className="flex items-center gap-4">
            <div className={`px-4 py-2 rounded-full font-medium flex items-center gap-2 ${status.lockConnected ? 'bg-emerald-500/10 text-emerald-400 border border-emerald-500/20' : 'bg-red-500/10 text-red-500 border border-red-500/20'}`} title="Lock Module">
              <KeyRound size={16} />
              {status.lockConnected ? 'Lock Online' : 'Lock Offline'}
            </div>
            <div className={`px-4 py-2 rounded-full font-medium flex items-center gap-2 ${status.camConnected ? 'bg-emerald-500/10 text-emerald-400 border border-emerald-500/20' : 'bg-red-500/10 text-red-500 border border-red-500/20'}`} title="Camera Module">
              <Camera size={16} />
              {status.camConnected ? 'Cam Online' : 'Cam Offline'}
            </div>
            <div className={`px-4 py-2 rounded-full font-medium flex items-center gap-2 ${status.pendingCommand === 'NONE' ? 'bg-blue-500/10 text-blue-400 border border-blue-500/20' : 'bg-yellow-500/10 text-yellow-400 border border-yellow-500/20'}`}>
              <Server size={18} />
              {status.pendingCommand === 'NONE' ? 'System Idle' : `Hardware Busy: ${status.pendingCommand}`}
            </div>
            {status.pendingCommand !== 'NONE' && (
              <button 
                onClick={handleClearCommand}
                className="px-3 py-2 bg-red-500/10 text-red-500 border border-red-500/20 hover:bg-red-500 hover:text-white transition-colors rounded-lg flex items-center gap-2 font-medium"
                title="Cancel ongoing hardware command"
              >
                <Trash2 size={16} /> Cancel
              </button>
            )}
          </div>
        </header>

        <div className="grid grid-cols-1 lg:grid-cols-3 gap-8">
          
          {/* Main Content (Users) */}
          <div className="lg:col-span-2 space-y-8">
            <div className="bg-slate-800 rounded-2xl p-6 shadow-xl border border-slate-700 relative overflow-hidden">
              <div className="flex justify-between items-center mb-6">
                <h2 className="text-xl font-semibold flex items-center gap-2">
                  <Users className="text-blue-400" />
                  Enrolled Fingerprints
                </h2>
                <div className="bg-slate-700/50 px-3 py-1 rounded-full text-xs font-medium text-slate-300">
                  {users.length} Total Records
                </div>
              </div>

              <div className="overflow-x-auto">
                <table className="w-full text-left border-collapse">
                  <thead>
                    <tr className="text-slate-400 text-xs uppercase tracking-wider border-b border-slate-700">
                      <th className="pb-3 pr-4">HW ID</th>
                      <th className="pb-3 px-4">Name</th>
                      <th className="pb-3 px-4">Role</th>
                      <th className="pb-3 px-4">Added</th>
                      <th className="pb-3 pl-4 text-right">Actions</th>
                    </tr>
                  </thead>
                  <tbody className="divide-y divide-slate-700/50">
                    {users.map(user => (
                      <tr key={user.id} className="hover:bg-slate-700/20 transition-colors">
                        <td className="py-4 pr-4">
                          <span className="bg-blue-500/10 text-blue-400 px-2 py-1 rounded font-mono text-sm border border-blue-500/20">
                            #{user.hardwareId}
                          </span>
                        </td>
                        <td className="py-4 px-4 font-medium">{user.name}</td>
                        <td className="py-4 px-4">
                          <span className={`px-2 py-1 rounded text-xs font-medium uppercase tracking-wider border ${user.role === 'ADMIN' ? 'bg-purple-500/10 text-purple-400 border-purple-500/20' : 'bg-slate-600/30 text-slate-300 border-slate-600/50'}`}>
                            {user.role}
                          </span>
                        </td>
                        <td className="py-4 px-4 text-sm text-slate-400">{new Date(user.dateAdded).toLocaleDateString()}</td>
                        <td className="py-4 pl-4 text-right">
                          <button onClick={() => handleDelete(user.id)} className="p-2 text-red-400 hover:bg-red-400/10 rounded-lg transition-colors border border-transparent hover:border-red-400/20" title="Revoke Access">
                            <Trash2 size={18} />
                          </button>
                        </td>
                      </tr>
                    ))}
                    {users.length === 0 && (
                      <tr>
                        <td colSpan="5" className="py-12 text-center text-slate-500">
                          <Fingerprint size={48} className="mx-auto mb-4 opacity-50" />
                          <p>No fingerprints enrolled yet.</p>
                        </td>
                      </tr>
                    )}
                  </tbody>
                </table>
              </div>
            </div>
            
            {/* Camera Stream */}
            <div className="bg-slate-800 rounded-2xl p-6 shadow-xl border border-slate-700">
              <h2 className="text-xl font-semibold flex items-center gap-2 mb-4">
                <Camera className="text-indigo-400" />
                Live Feed
              </h2>
              <div className="w-full aspect-video bg-black rounded-xl border border-slate-700 overflow-hidden flex items-center justify-center relative">
                {status.cameraIp ? (
                   <img 
                      src={`http://${status.cameraIp}:81/stream`} 
                      className="w-full h-full object-contain" 
                      alt="ESP32-CAM Stream"
                      onError={(e) => { e.target.style.display = 'none'; e.target.nextElementSibling.style.display = 'flex'; }}
                    />
                ) : (
                  <div className="flex flex-col items-center text-slate-500">
                    <Camera size={48} className="mb-2 opacity-50" />
                    <p>Camera offline</p>
                  </div>
                )}
                <div className="hidden absolute inset-0 flex-col items-center justify-center text-slate-500 bg-black">
                    <Camera size={48} className="mb-2 opacity-50" />
                    <p>Stream unavailable</p>
                </div>
              </div>
            </div>
          </div>

          {/* Sidebar */}
          <div className="space-y-8">
            
            {/* Enrollment */}
            <div className="bg-gradient-to-br from-blue-900/40 to-indigo-900/40 rounded-2xl p-6 shadow-xl border border-blue-500/20 relative overflow-hidden">
              <div className="absolute top-0 right-0 p-4 opacity-10">
                <Fingerprint size={120} />
              </div>
              <h2 className="text-xl font-semibold mb-6 relative z-10 flex items-center gap-2 text-blue-100">
                <Fingerprint size={20} className="text-blue-400" />
                New Fingerprint
              </h2>
              <form onSubmit={handleEnroll} className="space-y-4 relative z-10">
                <div>
                  <label className="block text-sm font-medium text-blue-200 mb-1">Holder Name</label>
                  <input 
                    type="text" 
                    required
                    value={enrollForm.name}
                    onChange={e => setEnrollForm({...enrollForm, name: e.target.value})}
                    className="w-full bg-slate-900/50 border border-blue-500/30 rounded-lg px-4 py-2.5 text-slate-100 focus:outline-none focus:border-blue-400 transition-colors placeholder-slate-500"
                    placeholder="E.g. John Doe"
                  />
                </div>
                <div>
                  <label className="block text-sm font-medium text-blue-200 mb-1">Access Level</label>
                  <select 
                    value={enrollForm.role}
                    onChange={e => setEnrollForm({...enrollForm, role: e.target.value})}
                    className="w-full bg-slate-900/50 border border-blue-500/30 rounded-lg px-4 py-2.5 text-slate-100 focus:outline-none focus:border-blue-400 transition-colors"
                  >
                    <option value="USER">Standard User (Slots 11-256)</option>
                    <option value="ADMIN">Admin (Slots 1-10)</option>
                  </select>
                </div>
                <button 
                  type="submit" 
                  disabled={status.pendingCommand !== 'NONE'}
                  className="w-full bg-blue-500 hover:bg-blue-400 text-white font-medium py-2.5 rounded-lg transition-colors shadow-lg shadow-blue-500/20 disabled:opacity-50 disabled:cursor-not-allowed flex items-center justify-center gap-2"
                >
                  <Fingerprint size={18} />
                  Start Enrollment
                </button>
              </form>
            </div>

            {/* Hardware PIN */}
            <div className="bg-slate-800 rounded-2xl p-6 shadow-xl border border-slate-700">
              <h2 className="text-xl font-semibold mb-6 flex items-center gap-2">
                <KeyRound size={20} className="text-amber-400" />
                Hardware PIN Setup
              </h2>
              <p className="text-sm text-slate-400 mb-4">Set the fallback PIN required alongside fingerprints (2FA via Keypad).</p>
              <form onSubmit={handlePinUpdate} className="space-y-4">
                <div>
                  <label className="block text-sm font-medium text-slate-300 mb-1">PIN Type</label>
                  <select 
                    value={pinForm.pinType}
                    onChange={e => setPinForm({...pinForm, pinType: e.target.value})}
                    className="w-full bg-slate-900 border border-slate-600 rounded-lg px-4 py-2.5 text-slate-100 focus:outline-none focus:border-amber-400 transition-colors"
                  >
                    <option value="ADMIN">Admin PIN (Access Menus + Unlock)</option>
                    <option value="USER">User PIN (Unlock Only)</option>
                  </select>
                </div>
                <div>
                  <label className="block text-sm font-medium text-slate-300 mb-1">New PIN</label>
                  <input 
                    type="password" 
                    required
                    maxLength="8"
                    value={pinForm.newPin}
                    onChange={e => setPinForm({...pinForm, newPin: e.target.value.replace(/[^0-9]/g, '')})}
                    className="w-full bg-slate-900 border border-slate-600 rounded-lg px-4 py-2.5 text-slate-100 focus:outline-none focus:border-amber-400 transition-colors font-mono tracking-widest placeholder-slate-500"
                    placeholder="Enter digits only..."
                  />
                </div>
                <button 
                  type="submit" 
                  disabled={status.pendingCommand !== 'NONE'}
                  className="w-full bg-slate-700 hover:bg-slate-600 text-white font-medium py-2.5 rounded-lg transition-colors disabled:opacity-50 disabled:cursor-not-allowed flex items-center justify-center gap-2"
                >
                  <KeyRound size={18} />
                  Update Lock PIN
                </button>
              </form>
            </div>

          </div>

        </div>
      </div>
    </div>
  )
}

export default App
