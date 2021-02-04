/*
 * Copyright (c) 2020 trinity-tech
 *
 * Permission is hereby granted, free of charge, to any person obtaining a copy
 * of this software and associated documentation files (the "Software"), to deal
 * in the Software without restriction, including without limitation the rights
 * to use, copy, modify, merge, publish, distribute, sublicense, and/or sell
 * copies of the Software, and to permit persons to whom the Software is
 * furnished to do so, subject to the following conditions:
 *
 * The above copyright notice and this permission notice shall be included in all
 * copies or substantial portions of the Software.
 *
 * THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR
 * IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
 * FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL THE
 * AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER
 * LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM,
 * OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN THE
 * SOFTWARE.
 */

#include <mutex>
#include <carrier.h>
#include <crystal.h>

#undef static_assert // fix double conflict between crystal and std functional
#include <CommandHandler.hpp>
#include "msgq.h"

int msgq_enq(const char *to, Marshalled *msg)
{
    auto data = std::vector<uint8_t>{ reinterpret_cast<uint8_t*>(msg->data),
                                      reinterpret_cast<uint8_t*>(msg->data) + msg->sz };
    int rc = trinity::CommandHandler::GetInstance()->send(to, data);
    return rc;
}