/******************************************************************************
 *
 *  This file is part of the Lynsyn PMU firmware.
 *
 *  Copyright 2018 Asbjørn Djupdal, NTNU, TULIPP EU Project
 *
 *  Lynsyn is free software: you can redistribute it and/or modify
 *  it under the terms of the GNU General Public License as published by
 *  the Free Software Foundation, either version 3 of the License, or
 *  (at your option) any later version.
 *
 *  Lynsyn is distributed in the hope that it will be useful,
 *  but WITHOUT ANY WARRANTY; without even the implied warranty of
 *  MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 *  GNU General Public License for more details.
 *
 *  You should have received a copy of the GNU General Public License
 *  along with Lynsyn.  If not, see <http://www.gnu.org/licenses/>.
 *
 *****************************************************************************/

`include "defines.v"

`timescale 1ns/1ps

`define ST_SPI_CMD               0
`define ST_SPI_STATUS            1
`define ST_SPI_MAGIC             2
`define ST_SPI_JTAG_SEL          3
`define ST_SPI_WR_SIZE           4
`define ST_SPI_WR_TDI_DATA       5
`define ST_SPI_WR_TMS_DATA       6
`define ST_SPI_GET_DATA          7
`define ST_SPI_STORE_SEQ_SIZE_L  8
`define ST_SPI_STORE_SEQ_SIZE_H  9
`define ST_SPI_STORE_SEQ        10
`define ST_SPI_STORE_PROG_SIZE  11
`define ST_SPI_STORE_PROG       12
`define ST_SPI_FLUSH            13
`define ST_SPI_JTAG_TEST        14
`define ST_SPI_OSC_TEST         15

module spi_controller
  (
   input wire        clk,
   input wire        rst,

   output wire       INT,
   input wire        SCLK,
   input wire        nSS,
   input wire        MOSI,
   output reg        MISO,
   
   output wire       out_seq_empty,
   output wire [4:0] out_seq_command,
   output wire [2:0] out_seq_bits,
   output wire [7:0] out_seq_tms,
   output wire [7:0] out_seq_tdi,
   output wire [7:0] out_seq_read,
   input wire        out_seq_re,

   output wire       in_seq_full,
   input wire        in_seq_we, 
   input wire [7:0]  in_seq_tdo,
   input wire        in_seq_flushed, 

   output reg [1:0]  jtag_sel = `JTAG_INT,

   output reg [4:0]  jtag_test_out = 0,
   input wire [4:0]  jtag_test_in,

   output reg        jtag_rst = 1
   );

   ///////////////////////////////////////////////////////////////////////////////

   wire [31:0]       out_seq_dout;
   wire              out_seq_full;
   reg               out_seq_we;
   reg [31:0]        out_seq_din;
   reg [7:0]         command;
   reg [7:0]         tdi;
   wire [7:0]        tms;
   reg [7:0]         read = 0;

   reg [3:0]         osc_test = 0;

   assign out_seq_tms = out_seq_dout[7:0];
   assign out_seq_tdi = out_seq_dout[15:8];
   assign out_seq_read = out_seq_dout[23:16];
   assign out_seq_bits = out_seq_dout[26:24];
   assign out_seq_command = out_seq_dout[31:27];

   wire              in_seq_empty;
   reg               in_seq_re;
   wire [7:0]        in_seq_tdo_dout;

   assign INT = in_seq_flushed;

   FIFO36E1 
     #(
       .DATA_WIDTH(36),
       .DO_REG(1),
       .EN_ECC_READ("FALSE"),
       .EN_ECC_WRITE("FALSE"),
       .EN_SYN("FALSE"),
       .FIFO_MODE("FIFO36"),
       .FIRST_WORD_FALL_THROUGH("TRUE"),
       .SIM_DEVICE("7SERIES")
       )
   out_seq_fifo
     (
      .DBITERR(),
      .ECCPARITY(),
      .SBITERR(),
      .DO(out_seq_dout),
      .DOP(),
      .ALMOSTEMPTY(),
      .ALMOSTFULL(),
      .EMPTY(out_seq_empty),
      .FULL(out_seq_full),
      .RDCOUNT(),
      .RDERR(),
      .WRCOUNT(),
      .WRERR(),
      .INJECTDBITERR(1'b0),
      .INJECTSBITERR(1'b0),
      .RDCLK(clk),
      .RDEN(out_seq_re),
      .REGCE(1'b1),
      .RST(jtag_rst | rst),
      .RSTREG(jtag_rst | rst),
      .WRCLK(SCLK),
      .WREN(out_seq_we),
      .DI(out_seq_din),
      .DIP(0)
      );

   FIFO36E1 
     #(
       .DATA_WIDTH(9),
       .DO_REG(1),
       .EN_ECC_READ("FALSE"),
       .EN_ECC_WRITE("FALSE"),
       .EN_SYN("FALSE"),
       .FIFO_MODE("FIFO36"),
       .FIRST_WORD_FALL_THROUGH("TRUE"),
       .SIM_DEVICE("7SERIES")
       )
   in_seq_fifo
     (
      .DBITERR(),
      .ECCPARITY(),
      .SBITERR(),
      .DO(in_seq_tdo_dout),
      .DOP(),
      .ALMOSTEMPTY(),
      .ALMOSTFULL(),
      .EMPTY(in_seq_empty),
      .FULL(in_seq_full),
      .RDCOUNT(),
      .RDERR(),
      .WRCOUNT(),
      .WRERR(),
      .INJECTDBITERR(1'b0),
      .INJECTSBITERR(1'b0),
      .RDCLK(SCLK),
      .RDEN(in_seq_re),
      .REGCE(1'b1),
      .RST(jtag_rst | rst),
      .RSTREG(jtag_rst | rst),
      .WRCLK(clk),
      .WREN(in_seq_we),
      .DI(in_seq_tdo),
      .DIP(0)
      );

   ///////////////////////////////////////////////////////////////////////////////

   always @(posedge clk) begin
      if(rst) begin
         osc_test <= 0;
      end else begin
         if(osc_test != 4'hf) osc_test <= osc_test + 1;
      end
   end

   ///////////////////////////////////////////////////////////////////////////////

   reg [7:0]   spi_in = 0;
   wire [7:0]  next_spi_in;
   reg [7:0]   spi_out = 0;
   reg [7:0]   spi_cnt = 1;

   reg         spi_word_ready = 0;

   reg [3:0]   spi_state  = `ST_SPI_CMD;
   reg [3:0]   next_spi_state;
   
   reg [12:0]  counter = 0;
   reg [15:0]  words;
   reg [12:0]  bytes;
   reg [2:0]   bits;

   reg [7:0]   size;

   wire [7:0]  status;

   reg         spi_seq_done = 0;

   assign next_spi_in = {spi_in[6:0],MOSI};
   assign tms = spi_in;

   assign status = {5'h0,out_seq_full,!in_seq_empty,in_seq_flushed};

   // change SPI output on falling edge
   always @(negedge SCLK) begin
      MISO <= spi_out[7];
   end

   always @(posedge SCLK) begin
      spi_state <= next_spi_state;

      spi_in <= next_spi_in;
      spi_out <= {spi_out[6:0],1'b0};
      spi_cnt <= {spi_cnt[6:0],spi_cnt[7]};
      spi_word_ready <= spi_cnt[7];

      if(spi_cnt[7]) begin
         spi_out <= status;
         if((spi_state == `ST_SPI_CMD) && (next_spi_in == `SPI_CMD_STATUS)) 
           spi_out <= status;
         if((spi_state == `ST_SPI_CMD) && (next_spi_in == `SPI_CMD_MAGIC)) 
           spi_out <= `MAGIC;
         if((spi_state == `ST_SPI_CMD) && (next_spi_in == `SPI_CMD_JTAG_TEST)) 
           spi_out <= jtag_test_in;
         if((spi_state == `ST_SPI_CMD) && (next_spi_in == `SPI_CMD_OSC_TEST)) begin
            spi_out[7:4] <= 0;
            spi_out[3:0] <= osc_test;
         end
         if(spi_state == `ST_SPI_JTAG_SEL) begin
            jtag_sel <= next_spi_in[1:0];
            jtag_test_out <= next_spi_in[6:2];
         end
         if((spi_state == `ST_SPI_CMD) && (next_spi_in == `SPI_CMD_WR_SEQ))
           read <= 0;
         if((spi_state == `ST_SPI_CMD) && (next_spi_in == `SPI_CMD_RDWR_SEQ))
           read <= 8'hff;
         if(spi_state == `ST_SPI_WR_SIZE) begin
            if(next_spi_in[2:0] == 0)
              bytes <= next_spi_in[7:3];
            else
              bytes <= next_spi_in[7:3] + 1;
            bits <= next_spi_in[2:0];
         end
         if(spi_state == `ST_SPI_STORE_SEQ) begin
            counter <= counter + 1;
         end
         if(spi_state == `ST_SPI_STORE_PROG) begin
            counter <= counter + 1;
         end
         if(spi_state == `ST_SPI_STORE_SEQ_SIZE_L) begin
            words[7:0] <= next_spi_in;
         end
         if(spi_state == `ST_SPI_STORE_SEQ_SIZE_H) begin
            words[15:8] <= next_spi_in;
         end
         if(spi_state == `ST_SPI_STORE_PROG_SIZE) begin
            words <= next_spi_in * 9;
         end
         if(((spi_state == `ST_SPI_CMD) && (next_spi_in == `SPI_CMD_GET_DATA)) || 
            (spi_state == `ST_SPI_GET_DATA)) begin
            spi_out <= in_seq_tdo_dout[7:0];
         end
         if(spi_state == `ST_SPI_WR_TDI_DATA) begin
            tdi = next_spi_in;
         end
         if(spi_state == `ST_SPI_WR_TMS_DATA) begin
            counter <= counter + 1;
         end
         if(spi_state == `ST_SPI_MAGIC) begin
            jtag_rst <= 0;
         end
      end
      if(spi_cnt[0]) begin
         if((spi_state != `ST_SPI_CMD) && (next_spi_state == `ST_SPI_CMD)) begin
            counter <= 0;
         end
      end
   end

   always @* begin
      next_spi_state <= spi_state;

      out_seq_we <= 0;
      in_seq_re <= 0;

      out_seq_din <= {`FIFO_CMD_WR,3'h0,read,tdi,tms};

      if(spi_word_ready) begin
        case(spi_state)
          `ST_SPI_CMD:
              case(spi_in)
                `SPI_CMD_STATUS:
                  next_spi_state <= `ST_SPI_STATUS;
                `SPI_CMD_MAGIC:
                  next_spi_state <= `ST_SPI_MAGIC;
                `SPI_CMD_JTAG_TEST:
                  next_spi_state <= `ST_SPI_JTAG_TEST;
                `SPI_CMD_OSC_TEST:
                  next_spi_state <= `ST_SPI_OSC_TEST;
                `SPI_CMD_JTAG_SEL:
                  next_spi_state <= `ST_SPI_JTAG_SEL;
                `SPI_CMD_WR_SEQ:
                  next_spi_state <= `ST_SPI_WR_SIZE;
                `SPI_CMD_RDWR_SEQ:
                  next_spi_state <= `ST_SPI_WR_SIZE;
                `SPI_CMD_GET_DATA: begin
                   in_seq_re <= 1;
                   next_spi_state <= `ST_SPI_GET_DATA;
                end
                `SPI_CMD_STORE_SEQ:
                  next_spi_state <= `ST_SPI_STORE_SEQ_SIZE_L;
                `SPI_CMD_STORE_PROG:
                  next_spi_state <= `ST_SPI_STORE_PROG_SIZE;
                `SPI_CMD_EXECUTE_SEQ: begin
                   out_seq_we <= 1;
                   out_seq_din <= {`FIFO_CMD_EXECUTE,27'h0};
                   next_spi_state <= `ST_SPI_FLUSH;
                end
              endcase
          `ST_SPI_STATUS:
            next_spi_state <= `ST_SPI_CMD;
          `ST_SPI_MAGIC:
            next_spi_state <= `ST_SPI_CMD;
          `ST_SPI_JTAG_TEST:
            next_spi_state <= `ST_SPI_CMD;
          `ST_SPI_OSC_TEST:
            next_spi_state <= `ST_SPI_CMD;
          `ST_SPI_JTAG_SEL:
            next_spi_state <= `ST_SPI_CMD;
          `ST_SPI_WR_SIZE:
            next_spi_state <= `ST_SPI_WR_TDI_DATA;
          `ST_SPI_WR_TDI_DATA:
            next_spi_state <= `ST_SPI_WR_TMS_DATA;
          `ST_SPI_WR_TMS_DATA: begin
             out_seq_we <= 1;
             next_spi_state <= `ST_SPI_WR_TDI_DATA;
             if(counter == bytes) begin
                out_seq_din <= {`FIFO_CMD_WR,bits,read,tdi,tms};
                next_spi_state <= `ST_SPI_FLUSH;
             end
          end
          `ST_SPI_STORE_SEQ_SIZE_L: begin
             next_spi_state <= `ST_SPI_STORE_SEQ_SIZE_H;
          end
          `ST_SPI_STORE_SEQ_SIZE_H: begin
             next_spi_state <= `ST_SPI_STORE_SEQ;
          end
          `ST_SPI_STORE_PROG_SIZE: begin
             next_spi_state <= `ST_SPI_STORE_PROG;
          end
          `ST_SPI_STORE_SEQ: begin
             out_seq_we <= 1;
             out_seq_din <= {`FIFO_CMD_STORE_SEQ,19'h0,spi_in};

             if(counter == words) begin
                next_spi_state <= `ST_SPI_FLUSH;
             end
          end
          `ST_SPI_STORE_PROG: begin
             out_seq_we <= 1;
             out_seq_din <= {`FIFO_CMD_STORE_PROG,19'h0,spi_in};

             if(counter == words) begin
                next_spi_state <= `ST_SPI_FLUSH;
             end
          end
          `ST_SPI_GET_DATA: begin
             if(spi_in == 8'hff) begin
                next_spi_state <= `ST_SPI_CMD;
             end
             in_seq_re <= 1;
          end
          `ST_SPI_FLUSH: 
            next_spi_state <= `ST_SPI_CMD;
          default:
            next_spi_state <= `ST_SPI_CMD;
        endcase

      end else begin
         case(spi_state)

           `ST_SPI_FLUSH: begin
              out_seq_we <= 1;
              out_seq_din <= {`FIFO_CMD_FLUSH,27'h0};
           end

         endcase
      end
   end

endmodule
