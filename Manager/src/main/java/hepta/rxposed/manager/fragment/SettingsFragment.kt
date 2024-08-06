/*
 * Copyright (C) 2018 The Android Open Source Project
 *
 * Licensed under the Apache License, Version 2.0 (the "License");
 * you may not use this file except in compliance with the License.
 * You may obtain a copy of the License at
 *
 *      http://www.apache.org/licenses/LICENSE-2.0
 *
 * Unless required by applicable law or agreed to in writing, software
 * distributed under the License is distributed on an "AS IS" BASIS,
 * WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
 * See the License for the specific language governing permissions and
 * limitations under the License.
 */

package hepta.rxposed.manager.fragment

import android.os.Bundle
import android.util.Log
import android.view.LayoutInflater
import android.view.Menu
import android.view.MenuInflater
import android.view.MenuItem
import android.view.View
import android.view.ViewGroup
import android.widget.EditText
import android.widget.Spinner
import androidx.core.view.MenuHost
import androidx.core.view.MenuProvider
import androidx.fragment.app.Fragment
import androidx.lifecycle.Lifecycle
import hepta.rxposed.manager.R
import hepta.rxposed.manager.util.InjectConfig

/**
 * A simple [Fragment] subclass.
 */
class SettingsFragment : Fragment() {

    override fun onCreateView(
            inflater: LayoutInflater,
            container: ViewGroup?,
            savedInstanceState: Bundle?
    ): View? {
        // Inflate the layout for this fragment
        return inflater.inflate(R.layout.fragment_settings, container, false)
    }

    override fun onViewCreated(view: View, savedInstanceState: Bundle?) {
        super.onViewCreated(view, savedInstanceState)

        init_ui()
        val menuHost: MenuHost = requireActivity()
        menuHost.addMenuProvider(object : MenuProvider {
            override fun onCreateMenu(menu: Menu, menuInflater: MenuInflater) {
                menuInflater.inflate(R.menu.setting_menu, menu)

            }

            override fun onMenuItemSelected(menuItem: MenuItem): Boolean {
                when(menuItem.itemId){
                    R.id.set_save->{
                        Log.e("rzx", "setting config verity")
                        InjectConfig.arm32_InjectSo = ""
                        InjectConfig.arm64_InjectSo = ""

                    }
                }
                return false
            }

        },viewLifecycleOwner, Lifecycle.State.RESUMED)

    }

    private fun init_ui() {
        val inject_type = view?.findViewById<Spinner>(R.id.inject_type)
        if (InjectConfig.hidemaps){
            inject_type?.setSelection(0)
        }else{
            inject_type?.setSelection(1)
        }
        if (InjectConfig.injectInit){
            inject_type?.setSelection(0)
        }else{
            inject_type?.setSelection(1)
        }
        val mountPath = view?.findViewById<EditText>(R.id.et_mount_path)
        val remarks = view?.findViewById<EditText>(R.id.et_remarks)
        val libPath = view?.findViewById<EditText>(R.id.et_lib_path)
        val lib64Path = view?.findViewById<EditText>(R.id.et_lib64_path)
        val injectArg = view?.findViewById<EditText>(R.id.et_inject_arg)
        remarks?.setText(InjectConfig.config_name)
        mountPath?.setText(InjectConfig.mountWorkDir)
        libPath?.setText(InjectConfig.arm32_InjectSo)
        lib64Path?.setText(InjectConfig.arm64_InjectSo)
        injectArg?.setText(InjectConfig.InjectArg)

        return
    }

}
